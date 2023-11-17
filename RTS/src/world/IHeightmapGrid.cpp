#include "stdafx.h"
#include "IHeightmapGrid.h"

#include "generation/IWorldGenerator.h"

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
//// TODO: We should make sure we dont build this on dedicated server as it initializes some memory
//struct patch_handle_pool {};
//using singleton_handle_pool = boost::singleton_pool<patch_handle_pool, sizeof(HeightmapPatchHandleData), boost::default_user_allocator_new_delete, boost::details::pool::default_mutex, 128u>;
//
//
//
//HeightmapPatchHandleData::HeightmapPatchHandleData() {
//
//}
//
//HeightmapPatchHandleData::~HeightmapPatchHandleData() {
//    // We cannot legally free until finished gen
//    assert(isFinishedGenerating);
//}
//
//void* HeightmapPatchHandleData::operator new(size_t count) {
//    UNUSED(count);
//    return singleton_handle_pool::malloc();
//}
//
//void HeightmapPatchHandleData::operator delete(void* pointer, size_t size) {
//    UNUSED(size);
//    return singleton_handle_pool::free(pointer);
//}

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


IHeightmapGrid::IHeightmapGrid(ui32 worldWidthTiles) : mWidthPatches(worldWidthTiles / HEIGHTMAP_WIDTH), mTotalPatches(SQ(mWidthPatches)) {
    mHeightData = std::unique_ptr<HeightmapPatch[]>(new HeightmapPatch[mTotalPatches]);
    mSpatialGrid2D.init(HEIGHTMAP_WIDTH, mWidthPatches);
    mMaxCoordinate = mWidthPatches * HEIGHTMAP_WIDTH - 1;
    mPatchWidth = (f32)worldWidthTiles / mWidthPatches;
}

IHeightmapGrid::~IHeightmapGrid() {
    // TODO: Unique_ptr?
    for (int i = 0; i < mTotalPatches; ++i) {
        delete mHeightData[i].mHeightData;
    }
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

const HeightmapPatchData* IHeightmapGrid::getHeightDataAtWorldPos(const i32v2& worldPos) const {
    return getHeightDataAt(mSpatialGrid2D.getIDAtWorldPos(worldPos));
}

const HeightmapPatchData* IHeightmapGrid::getHeightDataAt(HeightmapPatchID id) const {
    ASSERT_GAME_THREAD();
    const HeightmapPatch& patch = mHeightData[id];
    return patch.mHeightData;
}

void IHeightmapGrid::getPaddedHeightDataAt(HeightmapPatchID id, OUT const HeightmapPatchData* paddedHeightData[9]) {
    ASSERT_GAME_THREAD();
    HeightmapPatchID requiredIds[9];
    computeRequiredPaddedIDs(id, requiredIds);
    bool failed = false;
    for (int i = 0; i < 9; ++i) {
        if (mSpatialGrid2D.isIdValid(id)) {
            const HeightmapPatch& patch = mHeightData[requiredIds[i]];
            paddedHeightData[i] = patch.mHeightData;
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

void IHeightmapGrid::setHeightAtPatch(HeightmapPatchID patchId, ui32 vertIndex, f32 height, TerrainHeightSetDirection dir /*= TerrainHeightSetDirection::ANY*/)
{
    ASSERT_GAME_THREAD();
    setHeightAtInternal(patchId, vertIndex, height, dir);

    // Update duplicate verts (TODO: should we do this?)
    const ui32 x = vertIndex % HEIGHTMAP_VERT_WIDTH_PER_PATCH;
    const ui32 y = vertIndex / HEIGHTMAP_VERT_WIDTH_PER_PATCH;
    if (x == 0) {
        // update left duplicate verts
        const HeightmapPatchID leftId = mSpatialGrid2D.getWestID(patchId);
        const ui32 newIndex = vertIndex + HEIGHTMAP_VERT_WIDTH_PER_PATCH - 1;
        setHeightAtInternal(leftId, newIndex, height, dir);
        if (y == 0) {
            // update bottom left duplicate verts
            const ui32 cornerIndex = newIndex + HEIGHTMAP_VERT_SIZE_PER_PATCH - HEIGHTMAP_VERT_WIDTH_PER_PATCH;
            setHeightAtInternal(mSpatialGrid2D.getSouthID(leftId), cornerIndex, height, dir);
        }
        else if (y == HEIGHTMAP_QUAD_WIDTH_PER_PATCH) {
            // update top left duplicate verts
            const ui32 cornerIndex = newIndex - HEIGHTMAP_VERT_SIZE_PER_PATCH + HEIGHTMAP_VERT_WIDTH_PER_PATCH;
            setHeightAtInternal(mSpatialGrid2D.getNorthID(leftId), cornerIndex, height, dir);
        }
    }
    else if (x == HEIGHTMAP_QUAD_WIDTH_PER_PATCH) {
        // update right duplicate verts
        const HeightmapPatchID rightId = mSpatialGrid2D.getEastID(patchId);
        const ui32 newIndex = vertIndex - HEIGHTMAP_VERT_WIDTH_PER_PATCH + 1;
        setHeightAtInternal(rightId, newIndex, height, dir);
        if (y == 0) {
            // update bottom right duplicate verts
            const ui32 cornerIndex = newIndex + HEIGHTMAP_VERT_SIZE_PER_PATCH - HEIGHTMAP_VERT_WIDTH_PER_PATCH;
            setHeightAtInternal(mSpatialGrid2D.getSouthID(rightId), cornerIndex, height, dir);
        }
        else if (y == HEIGHTMAP_QUAD_WIDTH_PER_PATCH) {
            // update top right duplicate verts
            const ui32 cornerIndex = newIndex - HEIGHTMAP_VERT_SIZE_PER_PATCH + HEIGHTMAP_VERT_WIDTH_PER_PATCH;
            setHeightAtInternal(mSpatialGrid2D.getNorthID(rightId), cornerIndex, height, dir);
        }
    }
    if (y == 0) {
        // update bottom duplicate verts
        const HeightmapPatchID bottomId = mSpatialGrid2D.getSouthID(patchId);
        const ui32 newIndex = vertIndex + HEIGHTMAP_VERT_SIZE_PER_PATCH - HEIGHTMAP_VERT_WIDTH_PER_PATCH;
        setHeightAtInternal(bottomId, newIndex, height, dir);
    }
    else if (y == HEIGHTMAP_QUAD_WIDTH_PER_PATCH) {
        const HeightmapPatchID topId = mSpatialGrid2D.getNorthID(patchId);
        const ui32 newIndex = vertIndex - HEIGHTMAP_VERT_SIZE_PER_PATCH + HEIGHTMAP_VERT_WIDTH_PER_PATCH;
        setHeightAtInternal(topId, newIndex, height, dir);
    }
}

void IHeightmapGrid::adjustHeightAtChunk(ChunkID id, ui32 vertIndex, f32 adjust) {
    ASSERT_GAME_THREAD();
    return adjustHeightAtPatch(mSpatialGrid2D.getIDAtWorldPos(mWorld->getChunkGrid().getWorldPosXYFromChunkID(id)), vertIndex, adjust);
}

void IHeightmapGrid::adjustHeightAtPatch(HeightmapPatchID id, ui32 vertIndex, f32 adjust) {
    ASSERT_GAME_THREAD();
    assert(id < mTotalPatches);
    HeightmapPatch& patch = mHeightData[id];
    setHeightAtPatch(id, vertIndex, patch.mHeightData->data[vertIndex] + adjust);
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
    return patch.mHeightData->data[vertPos.y * HEIGHTMAP_VERT_WIDTH_PER_PATCH + vertPos.x];
}

f32 IHeightmapGrid::computeHeightAtPoint(const f32v2& worldPos) const {
    ASSERT_GAME_THREAD();
    HeightmapPatchID id = mSpatialGrid2D.getIDAtWorldPos(worldPos);
    const HeightmapPatch& patch = mHeightData[id];

    return computeHeightAtPoint(id, patch.mHeightData->data, worldPos);
}

f32 IHeightmapGrid::computeHeightAndNormalAtPoint(const f32v2& worldPos, OUT f32v3* outNormal) const {
    ASSERT_GAME_THREAD();
    HeightmapPatchID id = mSpatialGrid2D.getIDAtWorldPos(worldPos);
    const HeightmapPatch& patch = mHeightData[id];

    return computeHeightAndNormalAtPoint(id, patch.mHeightData->data, worldPos, outNormal);
}

f32 IHeightmapGrid::getHeightAtPointThreadSafe(const f32v2& worldPos) const
{
    const f32v2 clampedPos(glm::clamp(worldPos, 0.0f, mMaxCoordinate));

    HeightmapPatchID id = mSpatialGrid2D.getIDAtWorldPos(clampedPos);
    const HeightmapPatch& patch = mHeightData[id];

    std::shared_lock lock(patch.mHeightData->mMutex);
    return computeHeightAtPoint(id, patch.mHeightData->data, clampedPos);
}

f32 IHeightmapGrid::getHeightAndNormalAtPointThreadSafe(const f32v2& worldPos, OUT f32v3* outNormal) const
{
    const f32v2 clampedPos(glm::clamp(worldPos, 0.0f, mMaxCoordinate));

    ASSERT_GAME_THREAD();
    HeightmapPatchID id = mSpatialGrid2D.getIDAtWorldPos(clampedPos);
    const HeightmapPatch& patch = mHeightData[id];

    std::shared_lock lock(patch.mHeightData->mMutex);
    return computeHeightAndNormalAtPoint(id, patch.mHeightData->data, clampedPos, outNormal);
}

f32 IHeightmapGrid::computeHeightAtChunkOffset(const CompressedHeight* heightData, ChunkID chunkId, const f32v2& offsetIntoChunk) {
    i32v2 chunkPos = mWorld->getChunkGrid().getChunkOffsetFromChunkID(chunkId);
    // TODO: This doesnt account the individual world dimensions
    const f32v2 offset = offsetIntoChunk + f32v2((chunkPos % i32v2(HEIGHTMAP_PATCH_WIDTH_CHUNKS)) * (i32)CHUNK_WIDTH);
    const ui32v2 heightmapXY = ui32v2(ui32(offset.x / HEIGHTMAP_QUAD_SIZE), ui32(offset.y / HEIGHTMAP_QUAD_SIZE));

    // Compute normalized offset from bl
    // TODO: Optimize
    const f32v2 dxy = (offset - f32v2(heightmapXY) * (f32)HEIGHTMAP_QUAD_SIZE) / f32(HEIGHTMAP_QUAD_SIZE);
    assert(dxy.x >= 0.0f && dxy.x <= 1.0f && dxy.x >= 0.0f && dxy.x <= 1.0f);

    return interpolateHeightAtOffset(dxy, heightData, heightmapXY);
}

f32 IHeightmapGrid::computeHeightAtPoint(HeightmapPatchID id, const CompressedHeight* heightData, const f32v2& worldPos) const
{
    const f32v2 offset = worldPos - f32v2(mSpatialGrid2D.getWorldPosXYFromID(id));
    const ui32v2 heightmapXY = ui32v2(ui32(offset.x / HEIGHTMAP_QUAD_SIZE), ui32(offset.y / HEIGHTMAP_QUAD_SIZE));

    // Compute normalized offset from bl
    // TODO: Optimize
    const f32v2 dxy = (offset - f32v2(heightmapXY) * (f32)HEIGHTMAP_QUAD_SIZE) / f32(HEIGHTMAP_QUAD_SIZE);
    assert(dxy.x >= 0.0f && dxy.x <= 1.0f && dxy.x >= 0.0f && dxy.x <= 1.0f);

    return interpolateHeightAtOffset(dxy, heightData, heightmapXY);
}

f32 IHeightmapGrid::computeHeightAtPoint(const CompressedHeight* heightData, const f32v2& worldPos) const {
    return computeHeightAtPoint(mSpatialGrid2D.getIDAtWorldPos(i32v2(worldPos)), heightData, worldPos);
}

f32 IHeightmapGrid::computeHeightAndNormalAtPoint(HeightmapPatchID id, const CompressedHeight* heightData, const f32v2& worldPos, OUT f32v3* outNormal) const {
    assert(outNormal);
    const f32v2 offset = worldPos - f32v2(mSpatialGrid2D.getWorldPosXYFromID(id));
    const ui32v2 heightmapXY = ui32v2(ui32(offset.x / HEIGHTMAP_QUAD_SIZE), ui32(offset.y / HEIGHTMAP_QUAD_SIZE));

    // Compute normalized offset from bl
    // TODO: Optimize
    const f32v2 dxy = (offset - f32v2(heightmapXY) * (f32)HEIGHTMAP_QUAD_SIZE) / f32(HEIGHTMAP_QUAD_SIZE);
    assert(dxy.x >= 0.0f && dxy.x <= 1.0f && dxy.x >= 0.0f && dxy.x <= 1.0f);

    return getHeightAndNormalAtOffset(dxy, heightData, heightmapXY, outNormal);
}

f32 IHeightmapGrid::computeCenterHeightAtTile(const CompressedHeight* heightData, ui32v2 worldTilePos) const
{
    const f32v2 offset = getHeightmapOffsetFromTilePos(worldTilePos) + f32v2(0.5f);
    const ui32v2 heightmapXY = getHeightmapXYfromTilePos(worldTilePos);

    // Compute normalized offset from bl
    // TODO: Optimize
    const f32v2 dxy = (offset - f32v2(heightmapXY) * (f32)HEIGHTMAP_QUAD_SIZE) / f32(HEIGHTMAP_QUAD_SIZE);
    assert(dxy.x >= 0.0f && dxy.x <= 1.0f && dxy.x >= 0.0f && dxy.x <= 1.0f);

    return interpolateHeightAtOffset(dxy, heightData, heightmapXY);

}

f32 IHeightmapGrid::computeCenterHeightAtTile(ui32v2 worldTilePos) const {
    ASSERT_GAME_THREAD();

    HeightmapPatchID id = mSpatialGrid2D.getIDAtWorldPos(worldTilePos);
    const HeightmapPatch& patch = mHeightData[id];

    return computeCenterHeightAtTile(patch.mHeightData->data, worldTilePos);
}

void IHeightmapGrid::copyHeightRowToBuffer(CompressedHeight* dst, i32v2 worldPosStart, ui32 rowLength) const {
    ASSERT_GAME_THREAD();
    assert(mHeightData);
    assert(rowLength < HEIGHTMAP_VERT_WIDTH_PER_PATCH * 2.0f);

    ui32 lengthRemaining = rowLength;
    i32v2 worldPos = worldPosStart;
    do {
        // Get heightmap position and vertex offset
        HeightmapPatchID id = mSpatialGrid2D.getIDAtWorldPos(worldPos);
        i32v2 offset = (worldPos - mSpatialGrid2D.getWorldPosXYFromID(id)) / HEIGHTMAP_QUAD_SIZE;
        assert(offset.x < HEIGHTMAP_VERT_WIDTH_PER_PATCH);
        // Get length values
        const ui32 maxLength = HEIGHTMAP_VERT_WIDTH_PER_PATCH - offset.x;
        const ui32 lengthToCopy = glm::min(maxLength, lengthRemaining);
        // Copy data

        HeightmapPatchData* data = mHeightData[id].mHeightData;
        assert(data);
        memcpy(dst, &data->data[offset.y * HEIGHTMAP_VERT_WIDTH_PER_PATCH + offset.x], sizeof(CompressedHeight) * lengthToCopy);
        // Increment pointers and update length + position
        dst += lengthToCopy;
        lengthRemaining -= lengthToCopy;
        worldPos.x += HEIGHTMAP_QUAD_SIZE * lengthToCopy + 1; // +1 since we have a shared vertex on the edge
    } while (lengthRemaining > 0);
}

void IHeightmapGrid::computeTileCorners(const CompressedHeight* heightData, ui32v2 worldTilePos, OUT f32 corners[4]) const {
    constexpr f32 tileWidthHeightmap = 1.0f / HEIGHTMAP_QUAD_SIZE;

    f32v2 offset = getHeightmapOffsetFromTilePos(worldTilePos);
    ui32v2 heightmapXY = getHeightmapXYfromTilePos(worldTilePos);
    // Compute normalized offset from bl
    // TODO: Optimize
    f32v2 dxy = (offset - f32v2(heightmapXY) * (f32)HEIGHTMAP_QUAD_SIZE) / f32(HEIGHTMAP_QUAD_SIZE);
    assert(dxy.x >= 0.0f && dxy.x <= 1.0f && dxy.x >= 0.0f && dxy.x <= 1.0f);

    corners[0] = interpolateHeightAtOffset(dxy, heightData, heightmapXY);
    corners[1] = interpolateHeightAtOffset(dxy + f32v2(tileWidthHeightmap, 0.0f), heightData, heightmapXY);
    corners[2] = interpolateHeightAtOffset(dxy + f32v2(0.0f, tileWidthHeightmap), heightData, heightmapXY);
    corners[3] = interpolateHeightAtOffset(dxy + f32v2(tileWidthHeightmap), heightData, heightmapXY);
}

bool IHeightmapGrid::areTrianglesFlippedAtTile(const TileHandle& tileHandle) const {
    ASSERT_GAME_THREAD();
    i32v2 heightmapXY = tileHandle.getWorldPos2D() / (i32)HEIGHTMAP_QUAD_SIZE;
    return (heightmapXY.x + heightmapXY.y) % 2 == 1;
}

f32 IHeightmapGrid::computeMinHeightAtTile(const CompressedHeight* heightData, ui32v2 worldTilePos) const {
    f32 corners[4];
    computeTileCorners(heightData, worldTilePos, corners);
    return uncompressHeight(glm::min(glm::min(glm::min(corners[0], corners[1]), corners[2]), corners[3]));
}

f32 IHeightmapGrid::computeMinHeightAtTile(ui32v2 worldTilePos) const {
    ASSERT_GAME_THREAD();

    HeightmapPatchID id = mSpatialGrid2D.getIDAtWorldPos(worldTilePos);
    const HeightmapPatch& patch = mHeightData[id];

    return computeMinHeightAtTile(patch.mHeightData->data, worldTilePos);
}

f32 IHeightmapGrid::computeMaxHeightAtTile(ui32v2 worldTilePos) const {
    ASSERT_GAME_THREAD();
    HeightmapPatchID id = mSpatialGrid2D.getIDAtWorldPos(worldTilePos);
    const HeightmapPatch& patch = mHeightData[id];

    f32 corners[4];
    computeTileCorners(patch.mHeightData->data, worldTilePos, corners);
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
            meanHeight += computeHeightAtPoint(pos);
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
                meanHeight += computeHeightAtPoint(pos);
                ++total;
            }
        }
    }
    return meanHeight / (f32)total;
}


void IHeightmapGrid::onPatchFinishedGeneratingTODOREMOVE(HeightmapPatchID id) {
    ASSERT_GAME_THREAD();
    HeightmapPatch& patch = mHeightData[id];

    assert(!patch.mHeightData->mCollider);
    // Generate collider
    patch.mHeightData->mCollider = mWorld->getPhysicsWorld().addHeightField(patch);

}

void IHeightmapGrid::setHeightAtInternal(HeightmapPatchID id, ui32 vertIndex, f32 height, TerrainHeightSetDirection dir) {
    ASSERT_GAME_THREAD();
    HeightmapPatch& patch = mHeightData[id];
    {
        std::lock_guard lock(patch.mHeightData->mMutex);
        HeightmapPatchData& data = *patch.mHeightData;
        switch (dir) {
            case TerrainHeightSetDirection::ANY:
                data.setHeightAt(vertIndex, height);
                break;
            case TerrainHeightSetDirection::RAISE:
                if (height > uncompressHeight(data.data[vertIndex])) data.setHeightAt(vertIndex, height);
                break;
            case TerrainHeightSetDirection::LOWER:
                if (height < uncompressHeight(data.data[vertIndex])) data.setHeightAt(vertIndex, height);
                break;
            default:
                assert(false);
                break;

        }
        // TODO: Can we pull this out of critical section?
        // TODO: Update AABB/Sphere for dependencies such as terrain meshes?
        if (height > data.aabb.pos.z + data.aabb.height) {
            data.aabb.height = height - data.aabb.pos.z;
            data.boundingSphere = boundingSphereFromAABB(data.aabb);
        }
        else if (height < data.aabb.pos.z) {
            data.aabb.height += data.aabb.pos.z - height;
            data.aabb.pos.z = height;
            data.boundingSphere = boundingSphereFromAABB(data.aabb);
        }
    }
    // Store position of this edit for later batched notify
    ui32v2 worldPos = mSpatialGrid2D.getWorldPosXYFromID(id);
    const ui32 x = vertIndex % HEIGHTMAP_VERT_WIDTH_PER_PATCH;
    const ui32 y = vertIndex / HEIGHTMAP_VERT_WIDTH_PER_PATCH;
    worldPos.x += x * HEIGHTMAP_QUAD_SIZE;
    worldPos.y += y * HEIGHTMAP_QUAD_SIZE;
    mModifiedVertsThisTick.insert(i32v2(worldPos));
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

f32 IHeightmapGrid::getHeightAndNormalAtOffset(f32v2 dxy, const CompressedHeight* heightData, const ui32v2 heightmapXY, OUT f32v3* outNormal) {
    // Select which triangle we are looking at, taking into account orientation
    if ((heightmapXY.x + heightmapXY.y) % 2 == 0) {
        // This shape
        // **********
        // *     ** *
        // *   **   *
        // * **     *
        // **********
        if (dxy.x + (1.0f - dxy.y) > 1.0f) {
            // Lower quadrant
            // Get the 3 corner heights
            const f32 bl = uncompressHeight(heightData[heightmapXY.y * HEIGHTMAP_VERT_WIDTH_PER_PATCH + heightmapXY.x]);
            const f32 br = uncompressHeight(heightData[heightmapXY.y * HEIGHTMAP_VERT_WIDTH_PER_PATCH + heightmapXY.x + 1]);
            const f32 tr = uncompressHeight(heightData[(heightmapXY.y + 1) * HEIGHTMAP_VERT_WIDTH_PER_PATCH + heightmapXY.x + 1]);

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
            const f32 bl = uncompressHeight(heightData[heightmapXY.y * HEIGHTMAP_VERT_WIDTH_PER_PATCH + heightmapXY.x]);
            const f32 tl = uncompressHeight(heightData[(heightmapXY.y + 1) * HEIGHTMAP_VERT_WIDTH_PER_PATCH + heightmapXY.x]);
            const f32 tr = uncompressHeight(heightData[(heightmapXY.y + 1) * HEIGHTMAP_VERT_WIDTH_PER_PATCH + heightmapXY.x + 1]);

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
            const f32 br = uncompressHeight(heightData[heightmapXY.y * HEIGHTMAP_VERT_WIDTH_PER_PATCH + heightmapXY.x + 1]);
            const f32 tl = uncompressHeight(heightData[(heightmapXY.y + 1) * HEIGHTMAP_VERT_WIDTH_PER_PATCH + heightmapXY.x]);
            const f32 tr = uncompressHeight(heightData[(heightmapXY.y + 1) * HEIGHTMAP_VERT_WIDTH_PER_PATCH + heightmapXY.x + 1]);

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
            const f32 bl = uncompressHeight(heightData[heightmapXY.y * HEIGHTMAP_VERT_WIDTH_PER_PATCH + heightmapXY.x]);
            const f32 br = uncompressHeight(heightData[heightmapXY.y * HEIGHTMAP_VERT_WIDTH_PER_PATCH + heightmapXY.x + 1]);
            const f32 tl = uncompressHeight(heightData[(heightmapXY.y + 1) * HEIGHTMAP_VERT_WIDTH_PER_PATCH + heightmapXY.x]);

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

f32 IHeightmapGrid::interpolateHeightAtOffset(f32v2 dxy, const CompressedHeight* heightData, const ui32v2& heightmapXY) {
    // Select which triangle we are looking at, taking into account orientation
    if ((heightmapXY.x + heightmapXY.y) % 2 == 0) {
        // This shape
        // **********
        // *     ** *
        // *   **   *
        // * **     *
        // **********
        if (dxy.x + (1.0f - dxy.y) > 1.0f) {
            // Lower quadrant
            // Get the 3 corner heights
            const f32 bl = uncompressHeight(heightData[heightmapXY.y * HEIGHTMAP_VERT_WIDTH_PER_PATCH + heightmapXY.x]);
            const f32 br = uncompressHeight(heightData[heightmapXY.y * HEIGHTMAP_VERT_WIDTH_PER_PATCH + heightmapXY.x + 1]);
            const f32 tr = uncompressHeight(heightData[(heightmapXY.y + 1) * HEIGHTMAP_VERT_WIDTH_PER_PATCH + heightmapXY.x + 1]);

            const f32v3 uvw = BarycentricBlBrTr(dxy);
            return bl * uvw.x + br * uvw.y + tr * uvw.z;
        }
        else {
            // Upper quadrant
            // Get the 3 corner heights
            const f32 bl = uncompressHeight(heightData[heightmapXY.y * HEIGHTMAP_VERT_WIDTH_PER_PATCH + heightmapXY.x]);
            const f32 tl = uncompressHeight(heightData[(heightmapXY.y + 1) * HEIGHTMAP_VERT_WIDTH_PER_PATCH + heightmapXY.x]);
            const f32 tr = uncompressHeight(heightData[(heightmapXY.y + 1) * HEIGHTMAP_VERT_WIDTH_PER_PATCH + heightmapXY.x + 1]);

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
            const f32 br = uncompressHeight(heightData[heightmapXY.y * HEIGHTMAP_VERT_WIDTH_PER_PATCH + heightmapXY.x + 1]);
            const f32 tl = uncompressHeight(heightData[(heightmapXY.y + 1) * HEIGHTMAP_VERT_WIDTH_PER_PATCH + heightmapXY.x]);
            const f32 tr = uncompressHeight(heightData[(heightmapXY.y + 1) * HEIGHTMAP_VERT_WIDTH_PER_PATCH + heightmapXY.x + 1]);

            const f32v3 uvw = BarycentricBrTlTr(dxy);
            return br * uvw.x + tl * uvw.y + tr * uvw.z;
        }
        else {
            // Lower quadrant
            // Get the 3 corner heights
            const f32 bl = uncompressHeight(heightData[heightmapXY.y * HEIGHTMAP_VERT_WIDTH_PER_PATCH + heightmapXY.x]);
            const f32 br = uncompressHeight(heightData[heightmapXY.y * HEIGHTMAP_VERT_WIDTH_PER_PATCH + heightmapXY.x + 1]);
            const f32 tl = uncompressHeight(heightData[(heightmapXY.y + 1) * HEIGHTMAP_VERT_WIDTH_PER_PATCH + heightmapXY.x]);

            const f32v3 uvw = BarycentricBlBrTl(dxy);
            return bl * uvw.x + br * uvw.y + tl * uvw.z;
        }
    }
}

ui32v2 IHeightmapGrid::getHeightmapXYfromTilePos(ui32v2 worldTilePos) {
    ui32v2 heightmapXY = worldTilePos;
    // Offset into the heightmap by our chunk position
    heightmapXY = heightmapXY % ((ui32)CHUNK_WIDTH * HEIGHTMAP_PATCH_WIDTH_CHUNKS);
    heightmapXY /= HEIGHTMAP_QUAD_SIZE;
    return heightmapXY;
}

f32v2 IHeightmapGrid::getHeightmapOffsetFromTilePos(ui32v2 worldTilePos) {
    ui32v2 offset = worldTilePos;
    // Offset into the heightmap by our chunk position
    offset = offset % ((ui32)CHUNK_WIDTH * HEIGHTMAP_PATCH_WIDTH_CHUNKS);
    return f32v2(offset);
}
