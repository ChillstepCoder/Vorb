#include "stdafx.h"
#include "IHeightmapGrid.h"

#include "generation/ChunkGenerator.h"

#include "util/IntersectionUtil.h"

#include "camera/Camera3D.h"
#include "debugging/DebugRenderer.h"

// TODO: move
#include "rendering/ChunkGrassQuadtree.h"
#include "world/HeightmapTerrainQuadtree.h"
#include "physics/PhysicsWorld.h"

#include "world/World.h"
#include "world/IChunkGrid.h"

#include "util/BitArray.h"

#include "gamethread/GameThreadTasks.h"

#include <boost/pool/singleton_pool.hpp>

//
// https://gamedev.stackexchange.com/questions/23743/whats-the-most-efficient-way-to-find-barycentric-coordinates
// Compute barycentric coordinates (u, v, w) for
// point p with respect to triangle (a, b, c)
//void Barycentric3D(f32v3 p, f32v3 a, f32v3 b, f32v3 c, float& u, float& v, float& w)
//{
//    f32v3 v0 = b - a, v1 = c - a, v2 = p - a;
//    float d00 = glm::dot(v0, v0);
//    float d01 = glm::dot(v0, v1);
//    float d11 = glm::dot(v1, v1);
//    float d20 = glm::dot(v2, v0);
//    float d21 = glm::dot(v2, v1);
//    float denom = 1.0f / (d00 * d11 - d01 * d01);
//    v = (d11 * d20 - d01 * d21) * denom;
//    w = (d00 * d21 - d01 * d20) * denom;
//    u = 1.0f - v - w;
//}
void Barycentric2D(f32v2 p, f32v2 a, f32v2 b, f32v2 c, f32v3& uvw)
{
    f32v2 vb = b - a, vc = c - a, vp = p - a;
    float den = 1.0f / (vb.x * vc.y - vc.x * vb.y);
    uvw.y = (vp.x * vc.y - vc.x * vp.y) * den;
    uvw.z = (vb.x * vp.y - vp.x * vb.y) * den;
    uvw.x = 1.0f - uvw.y - uvw.z;
}

// Optimized for normalized p and hard coded a,b,c, derived from Barycentric2D
// Done by simply substituting the hardcoded constants and simplifying algebraically 
// f32v2(0.0f, 0.0f) /*bl*/, f32v2(1.0f, 0.0f) /*br*/, f32v2(1.0f, 1.0f) /*tr*/
inline f32v3 BarycentricBlBrTr(f32v2 p) {
    return f32v3(1.0f - p.x, p.x - p.y, p.y);
}

// f32v2(0.0f, 0.0f) /*bl*/, f32v2(0.0f, 1.0f) /*tl*/, f32v2(1.0f, 1.0f) /*tr*/
inline f32v3 BarycentricBlTlTr(f32v2 p) {
    return f32v3(1.0f - p.y, p.y - p.x, p.x);
}

// f32v2(1.0f, 0.0f) /*br*/, f32v2(0.0f, 1.0f) /*tl*/, f32v2(1.0f, 1.0f) /*tr*/
inline f32v3 BarycentricBrTlTr(f32v2 p) {
    return f32v3(1.0f - p.y, 1.0f - p.x, p.y + p.x - 1.0f);
}

// f32v2(0.0f, 0.0f) /*bl*/, f32v2(1.0f, 0.0f) /*br*/, f32v2(0.0f, 1.0f) /*tl*/
inline f32v3 BarycentricBlBrTl(f32v2 p) {
    return f32v3(1.0f - p.x - p.y, p.x, p.y);
}


IHeightmapGrid::IHeightmapGrid(ui32 worldWidthTiles) : mWidthPatches(worldWidthTiles / HEIGHTMAP_PATCH_WIDTH_TILES), mTotalPatches(SQ(mWidthPatches)) {
    initInternal();
}

IHeightmapGrid::~IHeightmapGrid() {

}

void IHeightmapGrid::tickShared() {
    ASSERT_GAME_THREAD();
    PROFILE_FUNCTION();

    // Notify of changed portions
    // TODO: Server Only
    if (mModifiedVertsThisTick.size()) {
        PROFILE_SCOPE("Dirty Heightmap");
        HeightmapGridEvent editEvent;
        editEvent.mEventType = HeightmapGridEventType::EditVerts;
        editEvent.mModifiedVerts = &mModifiedVertsThisTick;
        dispatchEditVerts(editEvent);
        mModifiedVertsThisTick.clear();
    }
}

// TODO: This could be optimized using a grid traversal algorithm instead of standard substepping
HeightmapPickResult IHeightmapGrid::pick(f32v3 rayStart, f32v3 rayEnd) {
    ASSERT_GAME_THREAD();
    PROFILE_FUNCTION();
    const f32v3 dir = rayEnd - rayStart;
    const f32 maxDist = glm::length(dir);
    const f32v3 normalizedDir = dir / maxDist;

    const f32 minStepLength = 0.1f;
    const f32 maxStepLength = HEIGHTMAP_QUAD_SIZE * 2.0f; // May cause tunneling but thats ok

    HeightmapPickResult result;
    f32v3 currentPoint = rayStart;

    f32 currDist = 0.0f;
    do {
        const f32 heightDiff = computeHeightAtPoint<false>(currentPoint) - currentPoint.z;
        if (heightDiff >= 0.0f) {
            result.hitPoint = currentPoint;
            computeHeightAndNormalAtPoint<false>(currentPoint, &result.hitNormal);
            result.hitTime = currDist / maxDist;
            result.didHit = true;
        }
        else {
            const f32 stepLength = glm::max(minStepLength, glm::min(maxStepLength, (-heightDiff) * 0.5f));
            currDist += stepLength;
            currentPoint += normalizedDir * stepLength;
        }
    } while (currDist <= maxDist && !result.didHit);

    return result;
}

const HeightmapPatch* IHeightmapGrid::getHeightDataAtWorldPos(const i32v2& worldPos) const {
    return getHeightDataAt(mSpatialGrid2D.getIDAtWorldPos(worldPos));
}

const HeightmapPatch* IHeightmapGrid::getHeightDataAt(HeightmapPatchID id) const {
    const HeightmapPatch& patch = mHeightData[id];
    return &patch;
}

HeightmapPatch& IHeightmapGrid::getPatchForGeneration(HeightmapPatchID id) {
    return mHeightData[id];
}

void IHeightmapGrid::getPaddedHeightDataAt(HeightmapPatchID id, OUT const HeightmapPatch* paddedHeightData[9]) {
    ASSERT_GAME_THREAD();
    HeightmapPatchID requiredIds[9];
    computeRequiredPaddedIDs(id, requiredIds);
    bool failed = false;
    for (int i = 0; i < 9; ++i) {
        if (mSpatialGrid2D.isIdValid(id)) {
            const HeightmapPatch& patch = mHeightData[requiredIds[i]];
            paddedHeightData[i] = &patch;
        }
        else {
            paddedHeightData[i] = nullptr;
        }
    }
}

void IHeightmapGrid::setHeightAtChunkId(ChunkID id, ui32 vertIndex, f32 height, TerrainHeightSetDirection dir/* = TerrainHeightSetDirection::ANY*/) {
    ASSERT_GAME_THREAD();
    return setHeightAtPatch(mSpatialGrid2D.getIDAtWorldPos(mWorld->getChunkGrid().getWorldPosXYFromChunkID(id)), vertIndex, height, dir);
}

void IHeightmapGrid::setHeightAtWorldPos(f32v2 worldPos, f32 height, TerrainHeightSetDirection dir /*= TerrainHeightSetDirection::ANY*/) {

    ASSERT_GAME_THREAD();
    const HeightmapPatchID id = mSpatialGrid2D.getIDAtWorldPos(worldPos);
    const i32v2 offset = i32v2(worldPos) - mSpatialGrid2D.getWorldPosXYFromID(id);
    const ui32 vertX = (ui32)offset.x / HEIGHTMAP_QUAD_SIZE;
    const ui32 vertY = (ui32)offset.y / HEIGHTMAP_QUAD_SIZE;
    const ui32 vertIndex = vertX + vertY * HEIGHTMAP_VERT_WIDTH_PER_PATCH;

    // TODO: Handle triangle rotation instead of always flattening the entire quad

    // Bottom left
    setHeightAtPatch(id, vertIndex, height, dir);
    // Bottom right
    if (vertX < HEIGHTMAP_QUAD_WIDTH_PER_PATCH) {
        setHeightAtPatch(id, vertIndex + 1, height, dir);
    }
    else {
        setHeightAtPatch(mSpatialGrid2D.getWestID(id), vertIndex - HEIGHTMAP_VERT_WIDTH_PER_PATCH + 1, height, dir);
    }

    // Top Left
    if (vertY < HEIGHTMAP_QUAD_WIDTH_PER_PATCH) {
        setHeightAtPatch(id, vertIndex + HEIGHTMAP_VERT_WIDTH_PER_PATCH, height, dir);
    }
    else {
        setHeightAtPatch(mSpatialGrid2D.getNorthID(id), vertIndex - HEIGHTMAP_VERT_SIZE_PER_PATCH + HEIGHTMAP_VERT_WIDTH_PER_PATCH, height, dir);
    }

    // Top Right
    if (vertX < HEIGHTMAP_QUAD_WIDTH_PER_PATCH && vertY < HEIGHTMAP_QUAD_WIDTH_PER_PATCH) {
        setHeightAtPatch(id, vertIndex + HEIGHTMAP_VERT_WIDTH_PER_PATCH + 1, height, dir);
    }
    else {
        HeightmapPatchID nextId = id;
        ui32 nextIndex = vertIndex;
        if (vertX < HEIGHTMAP_QUAD_WIDTH_PER_PATCH) {
            ++nextIndex;
        }
        else {
            nextIndex = nextIndex + 1 - HEIGHTMAP_VERT_WIDTH_PER_PATCH;
            nextId = mSpatialGrid2D.getEastID(nextId);
        }
        if (vertY < HEIGHTMAP_QUAD_WIDTH_PER_PATCH) {
            nextIndex += HEIGHTMAP_VERT_WIDTH_PER_PATCH;
        }
        else {
            nextIndex = nextIndex + HEIGHTMAP_VERT_WIDTH_PER_PATCH - HEIGHTMAP_VERT_SIZE_PER_PATCH;
            nextId = mSpatialGrid2D.getNorthID(nextId);
        }
        setHeightAtPatch(nextId, nextIndex, height, dir);
    }
}

void IHeightmapGrid::setHeightAtPatch(HeightmapPatchID patchId, ui32 vertIndex, f32 height, TerrainHeightSetDirection dir /*= TerrainHeightSetDirection::ANY*/) {
    ASSERT_GAME_THREAD();
    setHeightAtInternal(patchId, vertIndex, height, dir);
}

void IHeightmapGrid::adjustHeightAtChunk(ChunkID id, ui32 vertIndex, f32 adjust) {
    ASSERT_GAME_THREAD();
    return adjustHeightAtPatch(mSpatialGrid2D.getIDAtWorldPos(mWorld->getChunkGrid().getWorldPosXYFromChunkID(id)), vertIndex, adjust);
}

void IHeightmapGrid::adjustHeightAtPatch(HeightmapPatchID id, ui32 vertIndex, f32 adjust) {
    ASSERT_GAME_THREAD();
    assert(id < mTotalPatches);
    HeightmapPatch& patch = mHeightData[id];
    setHeightAtPatch(id, vertIndex, patch.getHeightAt(vertIndex) + adjust);
}

void IHeightmapGrid::markVertexDirty(HeightmapPatchID id, ui32 vertIndex) {
    ASSERT_GAME_THREAD();
    ui32v2 worldPos = mSpatialGrid2D.getWorldPosXYFromID(id);
    const ui32 x = vertIndex % HEIGHTMAP_VERT_WIDTH_PER_PATCH;
    const ui32 y = vertIndex / HEIGHTMAP_VERT_WIDTH_PER_PATCH;
    worldPos.x += x * HEIGHTMAP_QUAD_SIZE;
    worldPos.y += y * HEIGHTMAP_QUAD_SIZE;
    mModifiedVertsThisTick.insert(worldPos);
    mHeightData[id].isSaveUpToDate.clear();
}

void IHeightmapGrid::flattenAABB(const i32AABB2& aabb, f32 flattenHeight) {
    ASSERT_GAME_THREAD();
    std::set<ui32> dirtyChunks;
    for (i32 y = aabb.y; y <= aabb.y + aabb.dims.y; y += HEIGHTMAP_QUAD_SIZE) {
        for (i32 x = aabb.x; x <= aabb.x + aabb.dims.x; x += HEIGHTMAP_QUAD_SIZE) {
            const HeightmapPatchID id = mSpatialGrid2D.getIDAtWorldPos(i32v2(x, y));
            dirtyChunks.insert(id);
            const i32v2 worldPosChunk = mSpatialGrid2D.getWorldPosXYFromID(id);
            const i32v2 offset = i32v2(x, y) - worldPosChunk;
            const ui32 vertIndex = (ui32)offset.x / HEIGHTMAP_QUAD_SIZE + ((ui32)offset.y / HEIGHTMAP_QUAD_SIZE) * HEIGHTMAP_VERT_WIDTH_PER_PATCH;
            setHeightAtPatch(id, vertIndex, flattenHeight);
        }
    }
}

f32 IHeightmapGrid::getHeightAtVert(HeightmapPatchID id, const ui32v2& vertPos) const {
    ASSERT_GAME_THREAD();
    const HeightmapPatch& patch = mHeightData[id];
    assert(vertPos.x < HEIGHTMAP_VERT_WIDTH_PER_PATCH && vertPos.y < HEIGHTMAP_VERT_WIDTH_PER_PATCH);
    return patch.getHeightAt(vertPos.y * HEIGHTMAP_VERT_WIDTH_PER_PATCH + vertPos.x);
}

f32 IHeightmapGrid::getHeightAtVert(i32v2 vertPos) const {
    HeightmapPatchID id = mSpatialGrid2D.getIDfromGridXY(vertPos / HEIGHTMAP_VERT_WIDTH_PER_PATCH);
    vertPos.x = vertPos.x % HEIGHTMAP_VERT_WIDTH_PER_PATCH;
    vertPos.y = vertPos.y % HEIGHTMAP_VERT_WIDTH_PER_PATCH;
    return mHeightData[id].getHeightAt(vertPos.y * HEIGHTMAP_VERT_WIDTH_PER_PATCH + vertPos.x);
}

template <bool THREAD_SAFE>
f32 IHeightmapGrid::computeHeightAtPoint(const f32v2& worldPos) const {
    if constexpr (!THREAD_SAFE) ASSERT_GAME_THREAD();
    return interpolateHeightAtWorldPos<THREAD_SAFE>(worldPos);
}
DECL_BOOL_TEMPLATE(f32 IHeightmapGrid::computeHeightAtPoint, (const f32v2& worldPos) const)

f32 IHeightmapGrid::computeHeightAtPointForGeneration(const f32v2& worldPos) const {
    return interpolateHeightAtWorldPos<false>(worldPos);
}

f32 IHeightmapGrid::getHeightAtVertexForGeneration(const i32v2& worldVertexOffset) const {
    const i32v2 patchGridCoords(i32(worldVertexOffset.x) / HEIGHTMAP_QUAD_WIDTH_PER_PATCH, i32(worldVertexOffset.y) / HEIGHTMAP_QUAD_WIDTH_PER_PATCH);
    const HeightmapPatchID id = patchGridCoords.y * mWidthPatches + patchGridCoords.x;
    const i32v2 patchVertCoords = worldVertexOffset - patchGridCoords * HEIGHTMAP_QUAD_WIDTH_PER_PATCH;
    return mHeightData[id].getHeightAt(patchVertCoords.y * HEIGHTMAP_QUAD_WIDTH_PER_PATCH + patchVertCoords.x);
}

template <bool THREAD_SAFE>
f32 IHeightmapGrid::computeHeightAndNormalAtPoint(const f32v2& worldPos, OUT f32v3* outNormal) const {
    if constexpr (!THREAD_SAFE) ASSERT_GAME_THREAD();
    return interpolateHeightAndNormalAtWorldPos<THREAD_SAFE>(worldPos, outNormal);
}
DECL_BOOL_TEMPLATE(f32 IHeightmapGrid::computeHeightAndNormalAtPoint, (const f32v2& worldPos, OUT f32v3* outNormal) const)

template <bool THREAD_SAFE>
f32 IHeightmapGrid::computeCenterHeightAtTile(ui32v2 worldTilePos) const {
    if constexpr (!THREAD_SAFE) ASSERT_GAME_THREAD();
    return interpolateHeightAtWorldPos<THREAD_SAFE>(f32v2(worldTilePos) + f32v2(0.5f));
}
DECL_BOOL_TEMPLATE(f32 IHeightmapGrid::computeCenterHeightAtTile, (ui32v2 worldTilePos) const)

template <bool THREAD_SAFE>
f32 IHeightmapGrid::computeCenterHeightAndNormalAtTile(ui32v2 worldTilePos, OUT f32v3* outNormal) const {
    if constexpr (!THREAD_SAFE) ASSERT_GAME_THREAD();
    return interpolateHeightAndNormalAtWorldPos<THREAD_SAFE>(f32v2(worldTilePos) + f32v2(0.5f), outNormal);
}
DECL_BOOL_TEMPLATE(f32 IHeightmapGrid::computeCenterHeightAndNormalAtTile, (ui32v2 worldTilePos, OUT f32v3* outNormal) const)



//void IHeightmapGrid::copyHeightRowToBuffer(CompressedHeight* dst, i32v2 worldPosStart, ui32 rowLength) const {
//    ASSERT_GAME_THREAD();
//    assert(mHeightData);
//    assert(rowLength < HEIGHTMAP_VERT_WIDTH_PER_PATCH * 2.0f);
//
//    ui32 lengthRemaining = rowLength;
//    i32v2 worldPos = worldPosStart;
//    do {
//        // Get heightmap position and vertex offset
//        HeightmapPatchID id = mSpatialGrid2D.getIDAtWorldPos(worldPos);
//        i32v2 offset = (worldPos - mSpatialGrid2D.getWorldPosXYFromID(id)) / HEIGHTMAP_QUAD_SIZE;
//        assert(offset.x < HEIGHTMAP_VERT_WIDTH_PER_PATCH);
//        // Get length values
//        const ui32 maxLength = HEIGHTMAP_VERT_WIDTH_PER_PATCH - offset.x;
//        const ui32 lengthToCopy = glm::min(maxLength, lengthRemaining);
//        // Copy data
//
//        HeightmapPatchData* data = mHeightData[id].mHeightData;
//        assert(data);
//        memcpy(dst, &data->data[offset.y * HEIGHTMAP_VERT_WIDTH_PER_PATCH + offset.x], sizeof(CompressedHeight) * lengthToCopy);
//        // Increment pointers and update length + position
//        dst += lengthToCopy;
//        lengthRemaining -= lengthToCopy;
//        worldPos.x += HEIGHTMAP_QUAD_SIZE * lengthToCopy + 1; // +1 since we have a shared vertex on the edge
//    } while (lengthRemaining > 0);
//}

void IHeightmapGrid::computeTileCorners(ui32v2 worldTilePos, OUT f32 corners[4]) const {
    const f32v2 worldPosf(worldTilePos);
    corners[0] = interpolateHeightAtWorldPos<false>(worldPosf);
    corners[1] = interpolateHeightAtWorldPos<false>(worldPosf + f32v2(1.0f, 0.0f));
    corners[2] = interpolateHeightAtWorldPos<false>(worldPosf + f32v2(0.0f, 1.0f));
    corners[3] = interpolateHeightAtWorldPos<false>(worldPosf + f32v2(1.0f));
}

bool IHeightmapGrid::areTrianglesFlippedAtTile(const TileHandle& tileHandle) const {
    ASSERT_GAME_THREAD();
    i32v2 heightmapXY = tileHandle.getWorldPos2D() / (i32)HEIGHTMAP_QUAD_SIZE;
    return (heightmapXY.x + heightmapXY.y) % 2 == 1;
}

f32 IHeightmapGrid::computeMinHeightAtTile(ui32v2 worldTilePos) const {
    ASSERT_GAME_THREAD();
    f32 corners[4];
    computeTileCorners(worldTilePos, corners);
    return glm::min(glm::min(glm::min(corners[0], corners[1]), corners[2]), corners[3]);
}

f32 IHeightmapGrid::computeMaxHeightAtTile(ui32v2 worldTilePos) const {
    ASSERT_GAME_THREAD();
    f32 corners[4];
    computeTileCorners(worldTilePos, corners);
    return glm::max(glm::max(glm::max(corners[0], corners[1]), corners[2]), corners[3]);
}

f32 IHeightmapGrid::computeMeanHeightAtAABB(const i32AABB2& aabb) const {
    ASSERT_GAME_THREAD();
    // Compute mean height of height grid
    f32 meanHeight = 0.0f;
    const f32 total = (f32)(aabb.dims.x * aabb.dims.y);
    for (i32 y = 0; y < aabb.dims.y; ++y) {
        for (i32 x = 0; x < aabb.dims.x; ++x) {
            const f32v2 pos(aabb.x + x + 0.5f, aabb.y + y + 0.5f);
            meanHeight += computeHeightAtPoint<false>(pos);
        }
    }
    return meanHeight / total;
}

f32 IHeightmapGrid::computeMeanHeightAtAABB(const i32AABB2& aabb, const BitArray& checkBits) const {
    ASSERT_GAME_THREAD();
    // Compute mean height of height grid
    f32 meanHeight = 0.0f;
    ui32 total = 0;
    for (ui32 y = 0; y < aabb.dims.y; ++y) {
        for (ui32 x = 0; x < aabb.dims.x; ++x) {
            const ui32 index = y * aabb.dims.x + x;
            if (checkBits.getBit(index)) {
                const f32v2 pos(aabb.x + x + 0.5f, aabb.y + y + 0.5f);
                meanHeight += computeHeightAtPoint<false>(pos);
                ++total;
            }
        }
    }
    return meanHeight / (f32)total;
}

void IHeightmapGrid::initInternal() {
    assert(mWidthPatches);
    mTotalPatches = SQ(mWidthPatches);
    mHeightData = std::make_unique<HeightmapPatch[]>(mTotalPatches);
    for (HeightmapPatchID id = 0; id < mTotalPatches; ++id) {
        mHeightData[id].init(id);
    }
    mSpatialGrid2D.init(HEIGHTMAP_PATCH_WIDTH_TILES, mWidthPatches);
    mMaxCoordinate = mWidthPatches * HEIGHTMAP_PATCH_WIDTH_TILES - 1;
}

void IHeightmapGrid::setHeightAtInternal(HeightmapPatchID id, ui32 vertIndex, f32 height, TerrainHeightSetDirection dir) {
    ASSERT_GAME_THREAD();
    HeightmapPatch& patch = mHeightData[id];
    {
        std::lock_guard lock(patch.mMutex);
        switch (dir) {
            case TerrainHeightSetDirection::ANY:
                patch.setHeightAt(vertIndex, height);
                break;
            case TerrainHeightSetDirection::RAISE:
                if (height > uncompressHeight(patch.data[vertIndex])) patch.setHeightAt(vertIndex, height);
                break;
            case TerrainHeightSetDirection::LOWER:
                if (height < uncompressHeight(patch.data[vertIndex])) patch.setHeightAt(vertIndex, height);
                break;
            default:
                assert(false);
                break;

        }
        // TODO: Can we pull this out of critical section?
        // TODO: Update AABB/Sphere for dependencies such as terrain meshes?
        if (height > patch.aabb.pos.z + patch.aabb.height) {
            patch.aabb.height = height - patch.aabb.pos.z;
            patch.boundingSphere = boundingSphereFromAABB(patch.aabb);
        }
        else if (height < patch.aabb.pos.z) {
            patch.aabb.height += patch.aabb.pos.z - height;
            patch.aabb.pos.z = height;
            patch.boundingSphere = boundingSphereFromAABB(patch.aabb);
        }
    }
    // Store position of this edit for later batched notify
    markVertexDirty(id, vertIndex);
}

void IHeightmapGrid::computeRequiredPaddedIDs(HeightmapPatchID id, OUT HeightmapPatchID requiredIds[9]) const {
    HeightmapPatchID bottomId = mSpatialGrid2D.getSouthID(id);
    HeightmapPatchID topId = mSpatialGrid2D.getNorthID(id);
    requiredIds[0] = mSpatialGrid2D.getWestID(bottomId);
    requiredIds[1] = bottomId;
    requiredIds[2] = mSpatialGrid2D.getEastID(bottomId);
    requiredIds[3] = mSpatialGrid2D.getWestID(id);
    requiredIds[4] = id;
    requiredIds[5] = mSpatialGrid2D.getEastID(id);
    requiredIds[6] = mSpatialGrid2D.getWestID(topId);
    requiredIds[7] = topId;
    requiredIds[8] = mSpatialGrid2D.getEastID(topId);
}

template <bool THREAD_SAFE>
f32 IHeightmapGrid::interpolateHeightAndNormalAtWorldPos(f32v2 worldPos, OUT f32v3* outNormal) const {
    worldPos = glm::clamp(worldPos, 0.0f, mMaxCoordinate);

    // Get the position and ID of this patch
    const i32v2 patchGridCoords(i32(worldPos.x) / HEIGHTMAP_PATCH_WIDTH_TILES, i32(worldPos.y) / HEIGHTMAP_PATCH_WIDTH_TILES);
    const HeightmapPatchID blID = patchGridCoords.y * mWidthPatches + patchGridCoords.x;
    const i32v2 patchWorldCoords = patchGridCoords * HEIGHTMAP_PATCH_WIDTH_TILES;

    // Get offset into the patch and the xy indices of the current quad
    const f32v2 offsetIntoPatch = worldPos - f32v2(patchWorldCoords);
    const i32v2 quadXYIndices = i32v2(offsetIntoPatch) / HEIGHTMAP_QUAD_SIZE;
    const f32v2 quadRootPos = f32v2(quadXYIndices * HEIGHTMAP_QUAD_SIZE);

    // Get interpolation values between 0-1
    const f32v2 dxy = (offsetIntoPatch - quadRootPos) / f32(HEIGHTMAP_QUAD_SIZE);
    assert(dxy.x >= 0.0f && dxy.x <= 1.0f && dxy.x >= 0.0f && dxy.x <= 1.0f);

    const i32 rowStrideMinus1 = HEIGHTMAP_QUAD_WIDTH_PER_PATCH - 1;

    // Get patches and positions for each vertex
    int blIndex = quadXYIndices.y * HEIGHTMAP_QUAD_WIDTH_PER_PATCH + quadXYIndices.x;
    HeightmapPatchID brID = blID;
    int brIndex = blIndex + 1;
    HeightmapPatchID tlID = blID;
    int tlIndex = blIndex + HEIGHTMAP_QUAD_WIDTH_PER_PATCH;
    HeightmapPatchID trID = blID;
    int trIndex = blIndex + HEIGHTMAP_QUAD_WIDTH_PER_PATCH + 1;
    if (quadXYIndices.x >= rowStrideMinus1) [[unlikely]] {
        brID += 1;
        brIndex -= HEIGHTMAP_QUAD_WIDTH_PER_PATCH;
        trID += 1;
        trIndex -= HEIGHTMAP_QUAD_WIDTH_PER_PATCH;
        if (brID >= mTotalPatches) [[unlikely]] {
            brID = blID;
            brIndex = blIndex;
        }
        if (trID >= mTotalPatches) [[unlikely]] {
            trID = blID;
            trIndex = blIndex;
        }
    }
    if (quadXYIndices.y >= rowStrideMinus1) [[unlikely]] {
        tlID += mWidthPatches;
        tlIndex -= HEIGHTMAP_VERT_SIZE_PER_PATCH;
        trID += mWidthPatches;
        trIndex -= HEIGHTMAP_VERT_SIZE_PER_PATCH;
        if (tlID >= mTotalPatches) [[unlikely]] {
            tlID = blID;
            tlIndex = blIndex;
        }
        if (trID >= mTotalPatches) [[unlikely]] {
            trID = blID;
            trIndex = blIndex;
        }
    }
    // Select which triangle we are looking at, taking into account orientation
    if ((quadXYIndices.x + quadXYIndices.y) % 2 == 0) {
        // This shape
        // **********
        // *     ** *
        // *   **   *
        // * **     *
        // **********
        if (dxy.x + (1.0f - dxy.y) > 1.0f) {
            // Lower quadrant
            // Get the 3 corner heights
            const f32 bl = mHeightData[blID].getHeightAt<THREAD_SAFE>(blIndex);
            const f32 br = mHeightData[brID].getHeightAt<THREAD_SAFE>(brIndex);
            const f32 tr = mHeightData[trID].getHeightAt<THREAD_SAFE>(trIndex);

            // Compute normal from this triangle
            const f32v3 blv(0.0f, 0.0f, bl);
            const f32v3 brv(HEIGHTMAP_QUAD_SIZE, 0.0f, br);
            const f32v3 trv(HEIGHTMAP_QUAD_SIZE, HEIGHTMAP_QUAD_SIZE, tr);

            const f32v3 edge1 = blv - brv; // Edge 1
            const f32v3 edge2 = trv - brv; // Edge 2

            // Compute the normal
            const f32v3 normal = cross(edge2, edge1);
            *outNormal = normalize(normal); // Normalize the result to ensure it's a unit vector
            const f32v3 uvw = BarycentricBlBrTr(dxy);
            return bl * uvw.x + br * uvw.y + tr * uvw.z;
        }
        else {
            // Upper quadrant
            // Get the 3 corner heights
            const f32 bl = mHeightData[blID].getHeightAt<THREAD_SAFE>(blIndex);
            const f32 tl = mHeightData[tlID].getHeightAt<THREAD_SAFE>(tlIndex);
            const f32 tr = mHeightData[trID].getHeightAt<THREAD_SAFE>(trIndex);

            // Compute normal from this triangle
            const f32v3 blv(0.0f, 0.0f, bl);
            const f32v3 tlv(0.0f, HEIGHTMAP_QUAD_SIZE, tl);
            const f32v3 trv(HEIGHTMAP_QUAD_SIZE, HEIGHTMAP_QUAD_SIZE, tr);

            const f32v3 edge1 = trv - tlv; // Edge 1
            const f32v3 edge2 = blv - tlv; // Edge 2

            // Compute the normal
            const f32v3 normal = cross(edge2, edge1);
            *outNormal = normalize(normal); // Normalize the result to ensure it's a unit vector
            const f32v3 uvw = BarycentricBlTlTr(dxy);
            return bl * uvw.x + tl * uvw.y + tr * uvw.z;
        }
    }
    else {
        // This shape
        // **********
        // * **     *
        // *   **   *
        // *     ** *
        // **********
        if (dxy.x + dxy.y > 1.0f) {
            // Upper quadrant
            // Get the 3 corner heights
            const f32 br = mHeightData[brID].getHeightAt<THREAD_SAFE>(brIndex);
            const f32 tl = mHeightData[tlID].getHeightAt<THREAD_SAFE>(tlIndex);
            const f32 tr = mHeightData[trID].getHeightAt<THREAD_SAFE>(trIndex);

            // Compute normal from this triangle
            const f32v3 brv(HEIGHTMAP_QUAD_SIZE, 0.0f, br);
            const f32v3 tlv(0.0f, HEIGHTMAP_QUAD_SIZE, tl);
            const f32v3 trv(HEIGHTMAP_QUAD_SIZE, HEIGHTMAP_QUAD_SIZE, tr);

            const f32v3 edge1 = brv - trv; // Edge 1
            const f32v3 edge2 = tlv - trv; // Edge 2

            // Compute the normal
            const f32v3 normal = cross(edge2, edge1);
            *outNormal = normalize(normal); // Normalize the result to ensure it's a unit vector
            const f32v3 uvw = BarycentricBrTlTr(dxy);
            return br * uvw.x + tl * uvw.y + tr * uvw.z;
        }
        else {
            // Lower quadrant
            // Get the 3 corner heights
            const f32 bl = mHeightData[blID].getHeightAt<THREAD_SAFE>(blIndex);
            const f32 br = mHeightData[brID].getHeightAt<THREAD_SAFE>(brIndex);
            const f32 tl = mHeightData[tlID].getHeightAt<THREAD_SAFE>(tlIndex);

            // Compute normal from this triangle
            const f32v3 blv(0.0f, 0.0f, bl);
            const f32v3 brv(HEIGHTMAP_QUAD_SIZE, 0.0f, br);
            const f32v3 tlv(0.0f, HEIGHTMAP_QUAD_SIZE, tl);

            const f32v3 edge1 = tlv - blv; // Edge 1
            const f32v3 edge2 = brv - blv; // Edge 2

            // Compute the normal
            const f32v3 normal = cross(edge2, edge1);
            *outNormal = normalize(normal); // Normalize the result to ensure it's a unit vector
            const f32v3 uvw = BarycentricBlBrTl(dxy);
            return bl * uvw.x + br * uvw.y + tl * uvw.z;
        }
    }
}
DECL_BOOL_TEMPLATE(f32 IHeightmapGrid::interpolateHeightAndNormalAtWorldPos, (f32v2 worldPos, OUT f32v3* outNormal) const)

template <bool THREAD_SAFE>
f32 IHeightmapGrid::interpolateHeightAtWorldPos(f32v2 worldPos) const {
    worldPos = glm::clamp(worldPos, 0.0f, mMaxCoordinate);
    // Get the position and ID of this patch
    const i32v2 patchGridCoords(i32(worldPos.x) / HEIGHTMAP_PATCH_WIDTH_TILES, i32(worldPos.y) / HEIGHTMAP_PATCH_WIDTH_TILES);
    const HeightmapPatchID blID = patchGridCoords.y * mWidthPatches + patchGridCoords.x;
    const i32v2 patchWorldCoords = patchGridCoords * HEIGHTMAP_PATCH_WIDTH_TILES;

    // Get offset into the patch and the xy indices of the current quad
    const f32v2 offsetIntoPatch = worldPos - f32v2(patchWorldCoords);
    const i32v2 quadXYIndices = i32v2(offsetIntoPatch) / HEIGHTMAP_QUAD_SIZE;
    const f32v2 quadRootPos = f32v2(quadXYIndices * HEIGHTMAP_QUAD_SIZE);

    // Get interpolation values between 0-1
    const f32v2 dxy = (offsetIntoPatch - quadRootPos) / f32(HEIGHTMAP_QUAD_SIZE);
    assert(dxy.x >= 0.0f && dxy.x <= 1.0f && dxy.x >= 0.0f && dxy.x <= 1.0f);

    const i32 rowStrideMinus1 = HEIGHTMAP_QUAD_WIDTH_PER_PATCH - 1;

    // Get patches and positions for each vertex
    int blIndex = quadXYIndices.y * HEIGHTMAP_QUAD_WIDTH_PER_PATCH + quadXYIndices.x;
    HeightmapPatchID brID = blID;
    int brIndex = blIndex + 1;
    HeightmapPatchID tlID = blID;
    int tlIndex = blIndex + HEIGHTMAP_QUAD_WIDTH_PER_PATCH;
    HeightmapPatchID trID = blID;
    int trIndex = blIndex + HEIGHTMAP_QUAD_WIDTH_PER_PATCH + 1;
    if (quadXYIndices.x >= rowStrideMinus1) [[unlikely]] {
        brID += 1;
        brIndex -= HEIGHTMAP_QUAD_WIDTH_PER_PATCH;
        trID += 1;
        trIndex -= HEIGHTMAP_QUAD_WIDTH_PER_PATCH;
        if (brID >= mTotalPatches) [[unlikely]] {
            brID = blID;
            brIndex = blIndex;
        }
        if (trID >= mTotalPatches) [[unlikely]] {
            trID = blID;
            trIndex = blIndex;
        }
    }
    if (quadXYIndices.y >= rowStrideMinus1) [[unlikely]] {
        tlID += mWidthPatches;
        tlIndex -= HEIGHTMAP_VERT_SIZE_PER_PATCH;
        trID += mWidthPatches;
        trIndex -= HEIGHTMAP_VERT_SIZE_PER_PATCH;
        if (tlID >= mTotalPatches) [[unlikely]] {
            tlID = blID;
            tlIndex = blIndex;
        }
        if (trID >= mTotalPatches) [[unlikely]] {
            trID = blID;
            trIndex = blIndex;
        }
    }
    //const HeightmapPatchData* blPatch = mHeightData[rootId].mHeightData;
    // Select which triangle we are looking at, taking into account orientation
    if ((quadXYIndices.x + quadXYIndices.y) % 2 == 0) {
        // This shape
        // **********
        // *     ** *
        // *   **   *
        // * **     *
        // **********
        if (dxy.x + (1.0f - dxy.y) > 1.0f) {
            // Lower quadrant
            // Get the 3 corner heights
            const f32 bl = mHeightData[blID].getHeightAt<THREAD_SAFE>(blIndex);
            const f32 br = mHeightData[brID].getHeightAt<THREAD_SAFE>(brIndex);
            const f32 tr = mHeightData[trID].getHeightAt<THREAD_SAFE>(trIndex);

            const f32v3 uvw = BarycentricBlBrTr(dxy);
            return bl * uvw.x + br * uvw.y + tr * uvw.z;
        }
        else {
            // Upper quadrant
            // Get the 3 corner heights

            const f32 bl = mHeightData[blID].getHeightAt<THREAD_SAFE>(blIndex);
            const f32 tl = mHeightData[tlID].getHeightAt<THREAD_SAFE>(tlIndex);
            const f32 tr = mHeightData[trID].getHeightAt<THREAD_SAFE>(trIndex);

            const f32v3 uvw = BarycentricBlTlTr(dxy);
            return bl * uvw.x + tl * uvw.y + tr * uvw.z;
        }
    }
    else {
        // This shape
        // **********
        // * **     *
        // *   **   *
        // *     ** *
        // **********
        if (dxy.x + dxy.y > 1.0f) {
            // Upper quadrant
            // Get the 3 corner heights
            const f32 br = mHeightData[brID].getHeightAt<THREAD_SAFE>(brIndex);
            const f32 tl = mHeightData[tlID].getHeightAt<THREAD_SAFE>(tlIndex);
            const f32 tr = mHeightData[trID].getHeightAt<THREAD_SAFE>(trIndex);
         
            const f32v3 uvw = BarycentricBrTlTr(dxy);
            return br * uvw.x + tl * uvw.y + tr * uvw.z;
        }
        else {
            // Lower quadrant
            // Get the 3 corner heights

            const f32 bl = mHeightData[blID].getHeightAt<THREAD_SAFE>(blIndex);
            const f32 br = mHeightData[brID].getHeightAt<THREAD_SAFE>(brIndex);
            const f32 tl = mHeightData[tlID].getHeightAt<THREAD_SAFE>(tlIndex);
          
            const f32v3 uvw = BarycentricBlBrTl(dxy);
            return bl * uvw.x + br * uvw.y + tl * uvw.z;
        }
    }
}
