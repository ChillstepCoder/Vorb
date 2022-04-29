#include "stdafx.h"
#include "WorldGrid.h"

#include "generation/WorldGeneration.h"

#include "util/IntersectionUtil.h"

#include "camera/Camera3D.h"
#include "DebugRenderer.h"

// TODO: move
#include "rendering/ChunkGrassQuadtree.h"
#include "world/HeightmapTerrainQuadtree.h"

#include "World.h"

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

WorldGrid::WorldGrid(World& world) : mWorld(world) {
    for (ui32 i = 0; i < numChunks(); ++i) {
        mChunks[i].init(ChunkID(i), *this);
    }
}

void WorldGrid::requestHeightDataGenAndAquireAt(HeightmapPatchID id, std::function<void()> callback) {
    assert(IS_MAIN_THREAD());
    assert(id.id < WORLD_SIZE_HEIGHTMAP_PATCHES);
    HeightmapPatch& patch = mHeightData[id.id];
    assert(!patch.isDone());

    // Requesting generating locks the height data
    ++patch.mRefCount;

    if (!patch.mHeightData) {
        // TODO: Recycle
        patch.mHeightData = new HeightmapPatchData(id);
    }
    if (!patch.isGenerating()) {
        // Always extra ref while generating
        ++patch.mRefCount;
        patch.mFlags |= HEIGHTMAP_PATCH_FLAG_GENERATING;
        f32v2 position = id.getWorldPos();

        Services::Threadpool::ref().addTask([this, &patch, position](ThreadPoolWorkerData*) {
            generateHeightDataPatch(patch, position);
        }, [this, id]() {
            onPatchFinishedGenerating(id);
        });
    }
    if (callback) {
        mFinishCallbacks[id.id].push_back(callback);
    }
}

void WorldGrid::requestPaddedHeightDataGenAndAquireAt(HeightmapPatchID id, std::function<void()> callback) {
    assert(IS_MAIN_THREAD());
    HeightmapPatchID requiredIds[9];
    computeRequiredPaddedIDs(id, requiredIds);

    int totalRequired = 0;
    for (int i = 0; i < 9; ++i) {
        HeightmapPatchID requiredId = requiredIds[i];
        if (requiredId.isInvalid()) {
            continue;
        }
        HeightmapPatch& patch = mHeightData[requiredId.id];

        // Requesting generating locks the height data
        ++patch.mRefCount;

        if (patch.isDone()) {
            continue;
        }

        ++totalRequired;

        mPaddedGenListeners[requiredId.id].push_back(id);

        if (!patch.mHeightData) {
            // TODO: Recycle
            patch.mHeightData = new HeightmapPatchData(requiredId);
        }
        if (!patch.isGenerating()) {
            // Always extra ref while generating
            ++patch.mRefCount;
            patch.mFlags |= HEIGHTMAP_PATCH_FLAG_GENERATING;
            f32v2 position = requiredId.getWorldPos();

            Services::Threadpool::ref().addTask([this, &patch, position](ThreadPoolWorkerData*) {
                generateHeightDataPatch(patch, position);
            }, [this, requiredId]() {
                onPatchFinishedGenerating(requiredId);
            });
        }
    }

    if (callback) {
        if (totalRequired == 0) {
            callback();
        }
        else {
            // Keep track of how many neighbors we are waiting on to generate before callback runs
            assert(mPaddedGenWaitCount.find(id.id) == mPaddedGenWaitCount.end());
            mPaddedGenWaitCount[id.id] = totalRequired;
            mPaddedFinishCallbacks[id.id].push_back(callback);
        }
    }
    else if (totalRequired) {
        mPaddedGenWaitCount[id.id] = totalRequired;
    }
}

const HeightmapPatchData* WorldGrid::getHeightDataAt(HeightmapPatchID id) const {
    assert(IS_MAIN_THREAD());
    assert(mHeightData[id.id].isDone());
    return mHeightData[id.id].mHeightData;
}

const HeightmapPatchData* WorldGrid::tryGetHeightDataAt(HeightmapPatchID id) const {
    assert(IS_MAIN_THREAD());
    const HeightmapPatch& patch = mHeightData[id.id];
    if (patch.isDone()) {
        return patch.mHeightData;
    }
    return nullptr;
}

const HeightmapPatchData* WorldGrid::aquireHeightData(HeightmapPatchID id) {
    HeightmapPatch& patch = mHeightData[id.id];
    assert(patch.isDone());
    ++patch.mRefCount;
    return patch.mHeightData;
}

bool WorldGrid::tryAquirePaddedHeightDataAt(HeightmapPatchID id) {
    HeightmapPatchID requiredIds[9];
    computeRequiredPaddedIDs(id, requiredIds);
    bool failed = false;
    for (int i = 0; i < 9; ++i) {
        const HeightmapPatchID& required = requiredIds[i];
        if (!required.isInvalid()) {
            if (!mHeightData[required.id].isDone()) {
                failed = true;
                break;
            }
        }
    }
    if (failed) {
        return false;
    }
    else {
        // Return all data and aquire
        for (int i = 0; i < 9; ++i) {
            const HeightmapPatchID& required = requiredIds[i];
            if (!required.isInvalid()) {
                const HeightmapPatch& patch = mHeightData[required.id];
                ++mHeightData[required.id].mRefCount;
            }
        }
        return true;
    }
}

void WorldGrid::getPaddedHeightDataAt(HeightmapPatchID id, OUT const HeightmapPatchData* paddedHeightData[9]) {
    HeightmapPatchID requiredIds[9];
    computeRequiredPaddedIDs(id, requiredIds);
    bool failed = false;
    for (int i = 0; i < 9; ++i) {
        if (!id.isInvalid()) {
            const HeightmapPatch& patch = mHeightData[requiredIds[i].id];
            assert(patch.isDone());
            paddedHeightData[i] = patch.mHeightData;
        }
        else {
            paddedHeightData[i] = nullptr;
        }
    }
}

void WorldGrid::releaseHeightDataAt(HeightmapPatchID id) {
    // Padded may result in this
    if (id.isInvalid()) {
        return;
    }
    HeightmapPatch& patch = mHeightData[id.id];
    assert(patch.isDone() && patch.mRefCount);
    --patch.mRefCount;
    if (patch.mRefCount == 0) {
        patch.mFlags = 0u;
        delete patch.mHeightData; // TODO: Recycle
        patch.mHeightData = nullptr;
        // Remove from active list
        for (size_t i = 0; i < mActiveHeightmapPatches.size(); ++i) {
            if (mActiveHeightmapPatches[i] == id.id) {
                mActiveHeightmapPatches[i] = mActiveHeightmapPatches.back();
                mActiveHeightmapPatches.pop_back();
                break;
            }
        }
    }
}

void WorldGrid::releasePaddedHeightDataAt(HeightmapPatchID id) {
    // Release the 9 chunks
    HeightmapPatchID bottomId = id.getBottomID();
    HeightmapPatchID topId = id.getTopID();
    HeightmapPatchID requiredIDs[9];
    computeRequiredPaddedIDs(id, requiredIDs);
    for (int i = 0; i < 9; ++i) {
        releaseHeightDataAt(requiredIDs[i]);
    }
}

void WorldGrid::setHeightAt(ChunkID id, ui32 vertIndex, f32 height, TerrainHeightSetDirection dir/* = TerrainHeightSetDirection::ANY*/) {
    return setHeightAt(HeightmapPatchID(id.getWorldPos()), vertIndex, height, dir);
}

void WorldGrid::setHeightAt(f32v2 worldPos, f32 height, TerrainHeightSetDirection dir /*= TerrainHeightSetDirection::ANY*/) {

    const HeightmapPatchID id(worldPos);
    const f32v2 offset = worldPos - id.getWorldPos();
    const ui32 vertX = (ui32)offset.x / HEIGHTMAP_QUAD_SIZE;
    const ui32 vertY = (ui32)offset.y / HEIGHTMAP_QUAD_SIZE;
    const ui32 vertIndex = vertX + vertY * HEIGHTMAP_VERT_WIDTH_PER_PATCH;

    // TODO: Handle triangle rotation instead of always flattening the entire quad

    // Bottom left
    setHeightAt(id, vertIndex, height, dir);
    // Bottom right
    if (vertX < HEIGHTMAP_QUAD_WIDTH_PER_PATCH) {
        setHeightAt(id, vertIndex + 1, height, dir);
    }
    else {
        setHeightAt(id.getLeftID(), vertIndex - HEIGHTMAP_VERT_WIDTH_PER_PATCH + 1, height, dir);
    }

    // Top Left
    if (vertY < HEIGHTMAP_QUAD_WIDTH_PER_PATCH) {
        setHeightAt(id, vertIndex + HEIGHTMAP_VERT_WIDTH_PER_PATCH, height, dir);
    }
    else {
        setHeightAt(id.getTopID(), vertIndex - HEIGHTMAP_VERT_SIZE_PER_PATCH + HEIGHTMAP_VERT_WIDTH_PER_PATCH, height, dir);
    }

    // Top Right
    if (vertX < HEIGHTMAP_QUAD_WIDTH_PER_PATCH && vertY < HEIGHTMAP_QUAD_WIDTH_PER_PATCH) {
        setHeightAt(id, vertIndex + HEIGHTMAP_VERT_WIDTH_PER_PATCH + 1, height, dir);
    }
    else {
        HeightmapPatchID nextId = id;
        ui32 nextIndex = vertIndex;
        if (vertX < HEIGHTMAP_QUAD_WIDTH_PER_PATCH) {
            ++nextIndex;
        }
        else {
            nextIndex = nextIndex + 1 - HEIGHTMAP_VERT_WIDTH_PER_PATCH;
            nextId = nextId.getRightID();
        }
        if (vertY < HEIGHTMAP_QUAD_WIDTH_PER_PATCH) {
            nextIndex += HEIGHTMAP_VERT_WIDTH_PER_PATCH;
        }
        else {
            nextIndex = nextIndex + HEIGHTMAP_VERT_WIDTH_PER_PATCH - HEIGHTMAP_VERT_SIZE_PER_PATCH;
            nextId = nextId.getTopID();
        }
        setHeightAt(nextId, nextIndex, height, dir);
    }
}

void WorldGrid::setHeightAt(HeightmapPatchID patchId, ui32 vertIndex, f32 height, TerrainHeightSetDirection dir /*= TerrainHeightSetDirection::ANY*/)
{
    setHeightAtInternal(patchId, vertIndex, height, dir);

    // Update duplicate verts (TODO: should we do this?)
    const ui32 x = vertIndex % HEIGHTMAP_VERT_WIDTH_PER_PATCH;
    const ui32 y = vertIndex / HEIGHTMAP_VERT_WIDTH_PER_PATCH;
    if (x == 0) {
        // update left duplicate verts
        const HeightmapPatchID leftId = patchId.getLeftID();
        const ui32 newIndex = vertIndex + HEIGHTMAP_VERT_WIDTH_PER_PATCH - 1;
        setHeightAtInternal(leftId, newIndex, height, dir);
        if (y == 0) {
            // update bottom left duplicate verts
            const ui32 cornerIndex = newIndex + HEIGHTMAP_VERT_SIZE_PER_PATCH - HEIGHTMAP_VERT_WIDTH_PER_PATCH;
            setHeightAtInternal(leftId.getBottomID(), cornerIndex, height, dir);
        }
        else if (y == HEIGHTMAP_QUAD_WIDTH_PER_PATCH) {
            // update top left duplicate verts
            const ui32 cornerIndex = newIndex - HEIGHTMAP_VERT_SIZE_PER_PATCH + HEIGHTMAP_VERT_WIDTH_PER_PATCH;
            setHeightAtInternal(leftId.getTopID(), cornerIndex, height, dir);
        }
    }
    else if (x == HEIGHTMAP_QUAD_WIDTH_PER_PATCH) {
        // update right duplicate verts
        const HeightmapPatchID rightId = patchId.getRightID();
        const ui32 newIndex = vertIndex - HEIGHTMAP_VERT_WIDTH_PER_PATCH + 1;
        setHeightAtInternal(rightId, newIndex, height, dir);
        if (y == 0) {
            // update bottom right duplicate verts
            const ui32 cornerIndex = newIndex + HEIGHTMAP_VERT_SIZE_PER_PATCH - HEIGHTMAP_VERT_WIDTH_PER_PATCH;
            setHeightAtInternal(rightId.getBottomID(), cornerIndex, height, dir);
        }
        else if (y == HEIGHTMAP_QUAD_WIDTH_PER_PATCH) {
            // update top right duplicate verts
            const ui32 cornerIndex = newIndex - HEIGHTMAP_VERT_SIZE_PER_PATCH + HEIGHTMAP_VERT_WIDTH_PER_PATCH;
            setHeightAtInternal(rightId.getTopID(), cornerIndex, height, dir);
        }
    }
    if (y == 0) {
        // update bottom duplicate verts
        const HeightmapPatchID bottomId = patchId.getBottomID();
        const ui32 newIndex = vertIndex + HEIGHTMAP_VERT_SIZE_PER_PATCH - HEIGHTMAP_VERT_WIDTH_PER_PATCH;
        setHeightAtInternal(bottomId, newIndex, height, dir);
    }
    else if (y == HEIGHTMAP_QUAD_WIDTH_PER_PATCH) {
        const HeightmapPatchID topId = patchId.getTopID();
        const ui32 newIndex = vertIndex - HEIGHTMAP_VERT_SIZE_PER_PATCH + HEIGHTMAP_VERT_WIDTH_PER_PATCH;
        setHeightAtInternal(topId, newIndex, height, dir);
    }
}

void WorldGrid::adjustHeightAt(ChunkID id, ui32 vertIndex, f32 adjust) {
    return adjustHeightAt(HeightmapPatchID(id.getWorldPos()), vertIndex, adjust);
}

void WorldGrid::adjustHeightAt(HeightmapPatchID id, ui32 vertIndex, f32 adjust) {
    HeightmapPatch& patch = mHeightData[id.id];
    setHeightAt(id, vertIndex, patch.mHeightData->data[vertIndex] + adjust);
}

void WorldGrid::flattenAABB(const ui32AABB2& aabb, f32 flattenHeight) {
    std::set<ui32> dirtyChunks;
    for (ui32 y = aabb.y; y <= aabb.y + aabb.dims.y; y += HEIGHTMAP_QUAD_SIZE) {
        for (ui32 x = aabb.x; x <= aabb.x + aabb.dims.x; x += HEIGHTMAP_QUAD_SIZE) {
            HeightmapPatchID id(f32v2(x, y));
            dirtyChunks.insert(id.id);
            f32v2 worldPosChunk = id.getWorldPos();
            f32v2 offset = f32v2(x, y) - worldPosChunk;
            ui32 vertIndex = (ui32)offset.x / HEIGHTMAP_QUAD_SIZE + ((ui32)offset.y / HEIGHTMAP_QUAD_SIZE) * HEIGHTMAP_VERT_WIDTH_PER_PATCH;
            setHeightAt(id, vertIndex, flattenHeight);
        }
    }

    const f32v2 aabbCenter(aabb.getCenter());
    const f32 aabbDiagonalRadius = sqrt(SQ(aabb.dims.x * 0.5f) + SQ(aabb.dims.y * 0.5f));
    mWorld.dirtyTerrainFromBrush(aabbCenter, aabbDiagonalRadius);
}

f32 WorldGrid::getHeightAtVert(ChunkID id, const ui32v2& vertPos) const {
    const HeightmapPatch& patch = mHeightData[id.id];
    if (!patch.isDone()) return 0.0f;
    return patch.mHeightData->data[vertPos.y * HEIGHTMAP_VERT_WIDTH_PER_PATCH + vertPos.x];
}

bool WorldGrid::tryComputeHeightAtPoint(const f32v2& worldPos, f32* h) const {
    HeightmapPatchID id(worldPos);
    const HeightmapPatch& patch = mHeightData[id.id];

    if (!patch.isDone()) {
        return false;
    }

    *h = computeHeightAtPoint(id, patch.mHeightData->data, worldPos);
    return true;
}


f32 WorldGrid::tryComputeHeightAtPoint(const f32v2& worldPos) const {
    HeightmapPatchID id(worldPos);
    const HeightmapPatch& patch = mHeightData[id.id];

    if (!patch.isDone()) {
        return -100.0f;
    }

    return computeHeightAtPoint(id, patch.mHeightData->data, worldPos);
}

TerrainPickData WorldGrid::pickTerrainFromCameraVector(const Camera3D& camera, const f32v3& rayDir) const {
    const f32v3 rayStart = camera.getPosition();

    constexpr f32 RAY_CHECK_LENGTH = 10000.0f;
    constexpr f32 DEBUG_DURATION = 0.0f;
    const f32v3 rayEnd = rayStart + rayDir * RAY_CHECK_LENGTH;

    std::vector<std::pair<f32 /*closeTime*/, ui32> > sortedHits;

    sortedHits.reserve(30); // TODO: Prevent realloc
    for (ui32 i : mActiveHeightmapPatches) {
        const HeightmapPatch& patch = mHeightData[i];
        assert(patch.isDone());
        IntersectionHit3D sphereHit = IntersectionUtil::RaySphereIntersection(patch.mHeightData->boundingSphere, rayStart, rayDir);

        if (sphereHit.didHit()) {
            // Next do more expensive AABB hit
            IntersectionHit3D aabbHit = IntersectionUtil::LineAABBIntersection(patch.mHeightData->aabb, rayStart, rayEnd);
            if (aabbHit.didHit()) {
                sortedHits.push_back(std::make_pair(aabbHit.closeTime, i));
                // Uncomment to see size of sortedHits
                //DebugRenderer::drawWireQuad(ChunkID(i).getWorldPos(), f32v2(CHUNK_WIDTH), color4(1.0f, 1.0f, 0.0f, 1.0f), DEBUG_DURATION);
            }
        }
    }

    // Sort for nearest
    std::sort(sortedHits.begin(), sortedHits.end(), [](const std::pair<f32, ui32>& a, const std::pair<f32, ui32>& b) -> bool {
        return a.first < b.first;
    });

    for (auto&& hitPair : sortedHits) {
        const HeightmapPatch& patch = mHeightData[hitPair.second];
        f32v2 worldPos2D = HeightmapPatchID(hitPair.second).getWorldPos();
        // TODO: Instead of this, raymarch over the grid so we dont iterate the entire patch (BIG PERF)
        // This is currently a significant performance impact

        for (ui32 y = 0; y < HEIGHTMAP_QUAD_WIDTH_PER_PATCH; ++y) {
            for (ui32 x = 0; x < HEIGHTMAP_QUAD_WIDTH_PER_PATCH; ++x) {
                const ui32 blIndex = y * HEIGHTMAP_VERT_WIDTH_PER_PATCH + x;
                // TODO: Optimize
                const f32v3 v0 = f32v3(worldPos2D.x + x * HEIGHTMAP_QUAD_SIZE, worldPos2D.y + y * HEIGHTMAP_QUAD_SIZE, patch.mHeightData->data[blIndex]);
                const f32v3 v1 = f32v3(worldPos2D.x + (x + 1) * HEIGHTMAP_QUAD_SIZE, worldPos2D.y + y * HEIGHTMAP_QUAD_SIZE, patch.mHeightData->data[blIndex + 1]);
                const f32v3 v2 = f32v3(worldPos2D.x + x * HEIGHTMAP_QUAD_SIZE, worldPos2D.y + (y + 1) * HEIGHTMAP_QUAD_SIZE, patch.mHeightData->data[blIndex + HEIGHTMAP_VERT_WIDTH_PER_PATCH]);
                const f32v3 v3 = f32v3(worldPos2D.x + (x + 1) * HEIGHTMAP_QUAD_SIZE, worldPos2D.y + (y + 1) * HEIGHTMAP_QUAD_SIZE, patch.mHeightData->data[blIndex + HEIGHTMAP_VERT_WIDTH_PER_PATCH + 1]);
                if ((x + y) % 2) {
                    // 2********3
                    // *     ** *
                    // *   **   *
                    // * **     *
                    // 0********1
                    {
                        IntersectionHit3D hit = IntersectionUtil::RayTriangleIntersection(rayStart, rayDir, v0, v2, v3);
                        if (hit.didHit()) {
                            DebugRenderer::drawWireTriangle(v0, v2, v3, color4(1.0f, 0.0f, 0.0f, 1.0f), DEBUG_DURATION);
                            return TerrainPickData{ hitPair.second, patch.mHeightData->data, hit.position.z, blIndex, hit};
                        }
                    }
                    {
                        IntersectionHit3D hit = IntersectionUtil::RayTriangleIntersection(rayStart, rayDir, v0, v1, v3);
                        if (hit.didHit()) {
                            DebugRenderer::drawWireTriangle(v0, v1, v3, color4(1.0f, 0.0f, 0.0f, 1.0f), DEBUG_DURATION);
                            return TerrainPickData{ hitPair.second, patch.mHeightData->data, hit.position.z, blIndex, hit };
                        }
                    }
                }
                else {
                    // 2********3
                    // * **     *
                    // *   **   *
                    // *     ** *
                    // 0********1
                    {
                        IntersectionHit3D hit = IntersectionUtil::RayTriangleIntersection(rayStart, rayDir, v0, v1, v2);
                        if (hit.didHit()) {
                            DebugRenderer::drawWireTriangle(v0, v1, v2, color4(1.0f, 0.0f, 0.0f, 1.0f), DEBUG_DURATION);
                            return TerrainPickData{ hitPair.second, patch.mHeightData->data, hit.position.z, blIndex, hit };
                        }
                    }
                    {
                        IntersectionHit3D hit = IntersectionUtil::RayTriangleIntersection(rayStart, rayDir, v1, v2, v3);
                        if (hit.didHit()) {
                            DebugRenderer::drawWireTriangle(v1, v2, v3, color4(1.0f, 0.0f, 0.0f, 1.0f), DEBUG_DURATION);
                            return TerrainPickData{ hitPair.second, patch.mHeightData->data, hit.position.z, blIndex, hit };
                        }
                    }
                }
            }
        }
    }
    return TerrainPickData();
}

f32 WorldGrid::computeHeightAtPoint(HeightmapPatchID id, const f32* heightData, const f32v2& worldPos)
{
    const f32v2 offset = worldPos - id.getWorldPos();

    const ui32v2 heightmapXY = ui32v2(ui32(offset.x / HEIGHTMAP_QUAD_SIZE), ui32(offset.y / HEIGHTMAP_QUAD_SIZE));

    // Compute normalized offset from bl
    // TODO: Optimize
    const f32v2 dxy = (offset - f32v2(heightmapXY) * (f32)HEIGHTMAP_QUAD_SIZE) / f32(HEIGHTMAP_QUAD_SIZE);
    assert(dxy.x >= 0.0f && dxy.x <= 1.0f && dxy.x >= 0.0f && dxy.x <= 1.0f);

    return interpolateHeightAtOffset(dxy, heightData, heightmapXY);
}

f32 WorldGrid::computeHeightAtChunkOffset(const f32* heightData, ChunkID chunkId, const f32v2& chunkOffset)
{
    const f32v2 offset = chunkOffset + f32v2((chunkId.pos % HEIGHTMAP_PATCH_WIDTH_CHUNKS) * (ui32)CHUNK_WIDTH);
    const ui32v2 heightmapXY = ui32v2(ui32(offset.x / HEIGHTMAP_QUAD_SIZE), ui32(offset.y / HEIGHTMAP_QUAD_SIZE));

    // Compute normalized offset from bl
    // TODO: Optimize
    const f32v2 dxy = (offset - f32v2(heightmapXY) * (f32)HEIGHTMAP_QUAD_SIZE) / f32(HEIGHTMAP_QUAD_SIZE);
    assert(dxy.x >= 0.0f && dxy.x <= 1.0f && dxy.x >= 0.0f && dxy.x <= 1.0f);

    return interpolateHeightAtOffset(dxy, heightData, heightmapXY);
}

f32 WorldGrid::computeCenterHeightAtTile(const f32* heightData, TilePosition tilePos)
{
    const f32v2 offset = getHeightmapOffsetFromTilePos(tilePos) + f32v2(0.5f);
    const ui32v2 heightmapXY = getHeightmapXYfromTilePos(tilePos);

    // Compute normalized offset from bl
    // TODO: Optimize
    const f32v2 dxy = (offset - f32v2(heightmapXY) * (f32)HEIGHTMAP_QUAD_SIZE) / f32(HEIGHTMAP_QUAD_SIZE);
    assert(dxy.x >= 0.0f && dxy.x <= 1.0f && dxy.x >= 0.0f && dxy.x <= 1.0f);

    return interpolateHeightAtOffset(dxy, heightData, heightmapXY);

}

f32 WorldGrid::computeCenterHeightAtTile(TilePosition tilePos) const {

    HeightmapPatchID id = heightmapPatchIDFromChunkID(tilePos.chunkId);
    const HeightmapPatch& patch = mHeightData[id.id];

    if (!patch.isDone()) {
        return 0.0f;
    }

    return computeCenterHeightAtTile(patch.mHeightData->data, tilePos);
}

void WorldGrid::copyHeightRowToBuffer(f32* dst, ui32v2 worldPosStart, ui32 rowLength) const {
    assert(rowLength < HEIGHTMAP_VERT_WIDTH_PER_PATCH * 2.0f);

    ui32 lengthRemaining = rowLength;
    ui32v2 worldPos = worldPosStart;
    do {
        // Get heightmap position and vertex offset
        HeightmapPatchID id = HeightmapPatchID::fromWorldUI32v2(worldPos);
        ui32v2 offset = (worldPos - id.getWorldPosInt()) / HEIGHTMAP_QUAD_SIZE;
        assert(offset.x < HEIGHTMAP_VERT_WIDTH_PER_PATCH);
        // Get length values
        const ui32 maxLength = HEIGHTMAP_VERT_WIDTH_PER_PATCH - offset.x;
        const ui32 lengthToCopy = glm::min(maxLength, lengthRemaining);
        // Copy data
        HeightmapPatchData* data = mHeightData[id.id].mHeightData;
        assert(data);
        memcpy(dst, &data->data[offset.y * HEIGHTMAP_VERT_WIDTH_PER_PATCH + offset.x], sizeof(f32) * lengthToCopy);
        // Increment pointers and update length + position
        dst += lengthToCopy;
        lengthRemaining -= lengthToCopy;
        worldPos.x += HEIGHTMAP_QUAD_SIZE * lengthToCopy + 1; // +1 since we have a shared vertex on the edge
    } while (lengthRemaining > 0);
}

void WorldGrid::computeTileCorners(const f32* heightData, TilePosition tilePos, f32 corners[4]) {

    constexpr f32 tileWidthHeightmap = 1.0f / HEIGHTMAP_QUAD_SIZE;

    f32v2 offset = getHeightmapOffsetFromTilePos(tilePos);
    ui32v2 heightmapXY = getHeightmapXYfromTilePos(tilePos);
    // Compute normalized offset from bl
    // TODO: Optimize
    f32v2 dxy = (offset - f32v2(heightmapXY) * (f32)HEIGHTMAP_QUAD_SIZE) / f32(HEIGHTMAP_QUAD_SIZE);
    assert(dxy.x >= 0.0f && dxy.x <= 1.0f && dxy.x >= 0.0f && dxy.x <= 1.0f);

    corners[0] = interpolateHeightAtOffset(dxy, heightData, heightmapXY);
    corners[1] = interpolateHeightAtOffset(dxy + f32v2(tileWidthHeightmap, 0.0f), heightData, heightmapXY);
    corners[2] = interpolateHeightAtOffset(dxy + f32v2(0.0f, tileWidthHeightmap), heightData, heightmapXY);
    corners[3] = interpolateHeightAtOffset(dxy + f32v2(tileWidthHeightmap), heightData, heightmapXY);
}

bool WorldGrid::areTrianglesFlippedAtTile(TileIndex tileIndex) {
    ui32v2 heightmapXY = ui32v2(ui32(tileIndex.getX() / HEIGHTMAP_QUAD_SIZE), ui32(tileIndex.getY() / HEIGHTMAP_QUAD_SIZE));
    return (heightmapXY.x + heightmapXY.y) % 2 == 0;
}

f32 WorldGrid::computeMinHeightAtTile(const f32* heightData, TilePosition tilePos) {
    f32 corners[4];
    computeTileCorners(heightData, tilePos, corners);
    return glm::min(glm::min(glm::min(corners[0], corners[1]), corners[2]), corners[3]);
}

f32 WorldGrid::computeMinHeightAtTile(TilePosition tilePos) const {

    HeightmapPatchID id = heightmapPatchIDFromChunkID(tilePos.chunkId);
    const HeightmapPatch& patch = mHeightData[id.id];

    if (!patch.isDone()) {
        return 0.0f;
    }

    return computeMinHeightAtTile(patch.mHeightData->data, tilePos);
}

f32 WorldGrid::computeMaxHeightAtTile(TilePosition tilePos) const {
    HeightmapPatchID id = heightmapPatchIDFromChunkID(tilePos.chunkId);
    const HeightmapPatch& patch = mHeightData[id.id];

    if (!patch.isDone()) {
        return 0.0f;
    }

    f32 corners[4];
    computeTileCorners(patch.mHeightData->data, tilePos, corners);
    return glm::max(glm::max(glm::max(corners[0], corners[1]), corners[2]), corners[3]);
}

void WorldGrid::generateHeightDataPatch(HeightmapPatch& patch, const f32v2& position) {
    // AABB calculation
    f32AABB3& aabb = patch.mHeightData->aabb;
    aabb.dims.x = HEIGHTMAP_QUAD_SIZE * HEIGHTMAP_QUAD_WIDTH_PER_PATCH;
    aabb.dims.y = HEIGHTMAP_QUAD_SIZE * HEIGHTMAP_QUAD_WIDTH_PER_PATCH;
    aabb.pos.x = position.x;
    aabb.pos.y = position.y;
    f32 minZ = FLT_MAX;
    f32 maxZ = FLT_MIN;

    for (ui32 y = 0; y < HEIGHTMAP_VERT_WIDTH_PER_PATCH; ++y) {
        for (ui32 x = 0; x < HEIGHTMAP_VERT_WIDTH_PER_PATCH; ++x) {
            const f32v2 vertPos = f32v2(position.x + x * HEIGHTMAP_QUAD_SIZE, position.y + y * HEIGHTMAP_QUAD_SIZE);
            f32 height = sWorldGen.getHeightAtPos(vertPos);
            if (height > maxZ) maxZ = height;
            if (height < minZ) minZ = height;
            patch.mHeightData->data[y * HEIGHTMAP_VERT_WIDTH_PER_PATCH + x] = height;
        }
    }

    aabb.pos.z = minZ;
    aabb.dims.z = maxZ - minZ;
    patch.mHeightData->boundingSphere = boundingSphereFromAABB(aabb);
}


void WorldGrid::onPatchFinishedGenerating(HeightmapPatchID id) {
    HeightmapPatch& patch = mHeightData[id.id];
    mActiveHeightmapPatches.push_back(id.id);
    patch.mFlags = HEIGHTMAP_PATCH_FLAG_DONE;
    --patch.mRefCount;
    {
        auto&& it = mFinishCallbacks.find(id.id);
        if (it != mFinishCallbacks.end()) {
            for (auto&& func : it->second) {
                func();
            }
            mFinishCallbacks.erase(it);
        }
    }

    // See if any other patches were waiting on us for padded data access
    {
        auto&& it = mPaddedGenListeners.find(id.id);
        if (it != mPaddedGenListeners.end()) {
            //std::cout << "START " << it->second.size() << std::endl;
            for (auto&& idListener : it->second) {
                auto&& it2 = mPaddedGenWaitCount.find(idListener.id);
                assert(it2 != mPaddedGenWaitCount.end());
                //std::cout << it2->second << " " << idListener.id << std::endl;
                if (it2->second == 1) {
                    // Child is done, run any waiting callbacks
                    mPaddedGenWaitCount.erase(it2);
                    auto&& it3 = mPaddedFinishCallbacks.find(idListener.id);
                    if (it3 != mPaddedFinishCallbacks.end()) {
                        for (auto&& func : it3->second) {
                            func();
                        }
                        mPaddedFinishCallbacks.erase(it3);
                    }
                }
                else {
                    --it2->second;
                }
            }
            mPaddedGenListeners.erase(it);
        }
    }
}


void WorldGrid::setHeightAtInternal(HeightmapPatchID id, ui32 vertIndex, f32 height, TerrainHeightSetDirection dir) {
    HeightmapPatch& patch = mHeightData[id.id];
    if (patch.isDone() && patch.mRefCount) {
        HeightmapPatchData& data = *patch.mHeightData;
        switch (dir) {
            case TerrainHeightSetDirection::ANY:
                data.data[vertIndex] = height;
                break;
            case TerrainHeightSetDirection::RAISE:
                if (height > data.data[vertIndex]) data.data[vertIndex] = height;
                break;
            case TerrainHeightSetDirection::LOWER:
                if (height < data.data[vertIndex]) data.data[vertIndex] = height;
                break;
            default:
                assert(false);
                break;

        }
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
}

void WorldGrid::computeRequiredPaddedIDs(HeightmapPatchID id, OUT HeightmapPatchID requiredIds[9]) const {
    HeightmapPatchID bottomId = id.getBottomID();
    HeightmapPatchID topId = id.getTopID();
    requiredIds[0] = bottomId.getLeftID();
    requiredIds[1] = bottomId;
    requiredIds[2] = bottomId.getRightID();
    requiredIds[3] = id.getLeftID();
    requiredIds[4] = id;
    requiredIds[5] = id.getRightID();
    requiredIds[6] = topId.getLeftID();
    requiredIds[7] = topId;
    requiredIds[8] = topId.getRightID();
}

f32 WorldGrid::interpolateHeightAtOffset(f32v2 dxy, const f32* heightData, const ui32v2& heightmapXY) {
    // Select which triangle we are looking at, taking into account orientation
    if ((heightmapXY.x + heightmapXY.y) % 2) {
        // This shape
        // **********
        // *     ** *
        // *   **   *
        // * **     *
        // **********
        if (dxy.x + (1.0f - dxy.y) > 1.0f) {
            // Lower quadrant
            // Get the 3 corner heights
            const f32 bl = heightData[heightmapXY.y * HEIGHTMAP_VERT_WIDTH_PER_PATCH + heightmapXY.x];
            const f32 br = heightData[heightmapXY.y  * HEIGHTMAP_VERT_WIDTH_PER_PATCH + heightmapXY.x + 1];
            const f32 tr = heightData[(heightmapXY.y + 1) * HEIGHTMAP_VERT_WIDTH_PER_PATCH + heightmapXY.x + 1];

            const f32v3 uvw = BarycentricBlBrTr(dxy);
            return bl * uvw.x + br * uvw.y + tr * uvw.z;
        }
        else {
            // Upper quadrant
            // Get the 3 corner heights
            const f32 bl = heightData[heightmapXY.y * HEIGHTMAP_VERT_WIDTH_PER_PATCH + heightmapXY.x];
            const f32 tl = heightData[(heightmapXY.y + 1) * HEIGHTMAP_VERT_WIDTH_PER_PATCH + heightmapXY.x];
            const f32 tr = heightData[(heightmapXY.y + 1) * HEIGHTMAP_VERT_WIDTH_PER_PATCH + heightmapXY.x + 1];

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
            const f32 br = heightData[heightmapXY.y * HEIGHTMAP_VERT_WIDTH_PER_PATCH + heightmapXY.x + 1];
            const f32 tl = heightData[(heightmapXY.y + 1) * HEIGHTMAP_VERT_WIDTH_PER_PATCH + heightmapXY.x];
            const f32 tr = heightData[(heightmapXY.y + 1) * HEIGHTMAP_VERT_WIDTH_PER_PATCH + heightmapXY.x + 1];

            const f32v3 uvw = BarycentricBrTlTr(dxy);
            return br * uvw.x + tl * uvw.y + tr * uvw.z;
        }
        else {
            // Lower quadrant
            // Get the 3 corner heights
            const f32 bl = heightData[heightmapXY.y * HEIGHTMAP_VERT_WIDTH_PER_PATCH + heightmapXY.x];
            const f32 br = heightData[heightmapXY.y * HEIGHTMAP_VERT_WIDTH_PER_PATCH + heightmapXY.x + 1];
            const f32 tl = heightData[(heightmapXY.y + 1) * HEIGHTMAP_VERT_WIDTH_PER_PATCH + heightmapXY.x];

            const f32v3 uvw = BarycentricBlBrTl(dxy);
            return bl * uvw.x + br * uvw.y + tl * uvw.z;
        }
    }
}

ui32v2 WorldGrid::getHeightmapXYfromTilePos(TilePosition tilePos) {
    ui32v2 heightmapXY = ui32v2(tilePos.tileIndex.getX(), tilePos.tileIndex.getY());
    // Offset into the heightmap by our chunk position
    heightmapXY += (tilePos.chunkId.pos % HEIGHTMAP_PATCH_WIDTH_CHUNKS) * (ui32)CHUNK_WIDTH;
    heightmapXY /= HEIGHTMAP_QUAD_SIZE;
    return heightmapXY;
}

f32v2 WorldGrid::getHeightmapOffsetFromTilePos(TilePosition tilePos)
{
    f32v2 offset = f32v2(tilePos.tileIndex.getX(), tilePos.tileIndex.getY());
    // Offset into the heightmap by our chunk position
    offset += (tilePos.chunkId.pos % HEIGHTMAP_PATCH_WIDTH_CHUNKS) * (ui32)CHUNK_WIDTH;
    return offset;
}

HeightmapPatch::~HeightmapPatch() {
    delete[] mHeightData;
}
