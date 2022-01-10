#include "stdafx.h"
#include "WorldGrid.h"

#include "generation/WorldGeneration.h"

#include "services/Services.h"


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

WorldGrid::WorldGrid() {
    for (ui32 i = 0; i < numChunks(); ++i) {
        mChunks[i].init(ChunkID(i), *this);
    }
}

void WorldGrid::requestHeightDataGenAndAquireAt(ChunkID id, std::function<void()> callback) {
    assert(IS_MAIN_THREAD());
    HeightmapPatch& patch = mHeightData[id.id];
    assert(!patch.isDone());
    if (!patch.mHeightData) {
        // TODO: Recycle
        patch.mHeightData = new f32[HEIGHTMAP_VERT_SIZE_PER_CHUNK];
    }
    if (!patch.isGenerating()) {
        // Always extra ref while generating
        ++patch.mRefCount;
        patch.mFlags |= HEIGHTMAP_PATCH_FLAG_GENERATING;
        f32v2 position = id.getWorldPos();

        Services::Threadpool::ref().addTask([this, &patch, position](ThreadPoolWorkerData*) {
            for (ui32 y = 0; y < HEIGHTMAP_VERT_WIDTH_PER_CHUNK; ++y) {
                for (ui32 x = 0; x < HEIGHTMAP_VERT_WIDTH_PER_CHUNK; ++x) {
                    const f32v2 vertPos = f32v2(position.x + x * HEIGHTMAP_QUAD_SIZE, position.y + y * HEIGHTMAP_QUAD_SIZE);
                    f32 height = sWorldGen.getHeightAtPos(vertPos);
                    patch.mHeightData[y * HEIGHTMAP_VERT_WIDTH_PER_CHUNK + x] = height;
                }
            }
        }, [this, id]() {
            HeightmapPatch& patch = mHeightData[id.id];
            patch.mFlags = HEIGHTMAP_PATCH_FLAG_DONE;
            --patch.mRefCount;
            auto&& it = mFinishCallbacks.find(id);
            if (it != mFinishCallbacks.end()) {
                for (auto&& func : it->second) {
                    func();
                }
                mFinishCallbacks.erase(it);
            }
        });
    }
    if (callback) {
        mFinishCallbacks[id].push_back(callback);
    }
    // Requesting generating locks the height data
    ++patch.mRefCount;
}

const f32* WorldGrid::getHeightDataAt(ChunkID id) const {
    assert(IS_MAIN_THREAD());
    assert(mHeightData[id.id].isDone());
    return mHeightData[id.id].mHeightData;
}

const f32* WorldGrid::tryGetHeightDataAt(ChunkID id) const {
    assert(IS_MAIN_THREAD());
    const HeightmapPatch& patch = mHeightData[id.id];
    if (patch.isDone()) {
        return patch.mHeightData;
    }
    return nullptr;
}

const f32* WorldGrid::aquireHeightData(ChunkID id)
{
    HeightmapPatch& patch = mHeightData[id.id];
    assert(patch.isDone());
    ++patch.mRefCount;
    return patch.mHeightData;
}

void WorldGrid::releaseHeightDataAt(ChunkID id) {
    HeightmapPatch& patch = mHeightData[id.id];
    assert(patch.isDone() && patch.mRefCount);
    --patch.mRefCount;
    if (patch.mRefCount == 0) {
        patch.mFlags = 0u;
        delete[] patch.mHeightData; // TODO: Recycle
        patch.mHeightData = 0;
    }
}

bool WorldGrid::tryComputeHeightAtPoint(const f32v2& worldPos, f32* h) const {
    ChunkID id(worldPos);
    const HeightmapPatch& patch = mHeightData[id.id];

    if (!patch.isDone()) {
        return false;
    }

    *h = computeHeightAtPoint(id, patch.mHeightData, worldPos);
    return true;
}


f32 WorldGrid::computeHeightAtPoint(ChunkID id, const f32* heightData, const f32v2& worldPos)
{
    const f32v2 offset = worldPos - id.getWorldPos();

    const ui32v2 heightmapXY = ui32v2(ui32(offset.x / HEIGHTMAP_QUAD_SIZE), ui32(offset.y / HEIGHTMAP_QUAD_SIZE));

    // Compute normalized offset from bl
    // TODO: Optimize
    const f32v2 dxy = (offset - f32v2(heightmapXY) * (f32)HEIGHTMAP_QUAD_SIZE) / f32(HEIGHTMAP_QUAD_SIZE);
    assert(dxy.x >= 0.0f && dxy.x <= 1.0f && dxy.x >= 0.0f && dxy.x <= 1.0f);

    return interpolateHeightAtOffset(dxy, heightData, heightmapXY);
}

f32 WorldGrid::computeHeightAtChunkOffset(const f32* heightData, const f32v2& chunkOffset)
{
    const ui32v2 heightmapXY = ui32v2(ui32(chunkOffset.x / HEIGHTMAP_QUAD_SIZE), ui32(chunkOffset.y / HEIGHTMAP_QUAD_SIZE));

    // Compute normalized offset from bl
    // TODO: Optimize
    const f32v2 dxy = (chunkOffset - f32v2(heightmapXY) * (f32)HEIGHTMAP_QUAD_SIZE) / f32(HEIGHTMAP_QUAD_SIZE);
    assert(dxy.x >= 0.0f && dxy.x <= 1.0f && dxy.x >= 0.0f && dxy.x <= 1.0f);

    return interpolateHeightAtOffset(dxy, heightData, heightmapXY);
}

f32 WorldGrid::computeCenterHeightAtTile(const f32* heightData, TileIndex tileIndex)
{
    const f32v2 offset(tileIndex.getX() + 0.5f, tileIndex.getY() + 0.5f);

    const ui32v2 heightmapXY = ui32v2(ui32(offset.x / HEIGHTMAP_QUAD_SIZE), ui32(offset.y / HEIGHTMAP_QUAD_SIZE));

    // Compute normalized offset from bl
    // TODO: Optimize
    const f32v2 dxy = (offset - f32v2(heightmapXY) * (f32)HEIGHTMAP_QUAD_SIZE) / f32(HEIGHTMAP_QUAD_SIZE);
    assert(dxy.x >= 0.0f && dxy.x <= 1.0f && dxy.x >= 0.0f && dxy.x <= 1.0f);

    return interpolateHeightAtOffset(dxy, heightData, heightmapXY);

}

f32 WorldGrid::computeMinHeightAtTile(const f32* heightData, TileIndex tileIndex) {
    f32v2 offset(tileIndex.getX(), tileIndex.getY());

    ui32v2 heightmapXY = ui32v2(ui32(offset.x / HEIGHTMAP_QUAD_SIZE), ui32(offset.y / HEIGHTMAP_QUAD_SIZE));
    constexpr f32 tileWidthHeightmap = 1.0f / HEIGHTMAP_QUAD_SIZE;

    // Compute normalized offset from bl
    // TODO: Optimize
    f32v2 dxy = (offset - f32v2(heightmapXY) * (f32)HEIGHTMAP_QUAD_SIZE) / f32(HEIGHTMAP_QUAD_SIZE);
    assert(dxy.x >= 0.0f && dxy.x <= 1.0f && dxy.x >= 0.0f && dxy.x <= 1.0f);

    // Get the 4 
    const f32 bl = interpolateHeightAtOffset(dxy, heightData, heightmapXY);
    const f32 br = interpolateHeightAtOffset(dxy + f32v2(tileWidthHeightmap, 0.0f), heightData, heightmapXY);
    const f32 tl = interpolateHeightAtOffset(dxy + f32v2(0.0f, tileWidthHeightmap), heightData, heightmapXY);
    const f32 tr = interpolateHeightAtOffset(dxy + f32v2(tileWidthHeightmap), heightData, heightmapXY);

    return glm::min(glm::min(glm::min(bl, br), tl), tr);
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
            const f32 bl = heightData[heightmapXY.y * HEIGHTMAP_VERT_WIDTH_PER_CHUNK + heightmapXY.x];
            const f32 br = heightData[heightmapXY.y  * HEIGHTMAP_VERT_WIDTH_PER_CHUNK + heightmapXY.x + 1];
            const f32 tr = heightData[(heightmapXY.y + 1) * HEIGHTMAP_VERT_WIDTH_PER_CHUNK + heightmapXY.x + 1];

            const f32v3 uvw = BarycentricBlBrTr(dxy);
            return bl * uvw.x + br * uvw.y + tr * uvw.z;
        }
        else {
            // Upper quadrant
            // Get the 3 corner heights
            const f32 bl = heightData[heightmapXY.y * HEIGHTMAP_VERT_WIDTH_PER_CHUNK + heightmapXY.x];
            const f32 tl = heightData[(heightmapXY.y + 1) * HEIGHTMAP_VERT_WIDTH_PER_CHUNK + heightmapXY.x];
            const f32 tr = heightData[(heightmapXY.y + 1) * HEIGHTMAP_VERT_WIDTH_PER_CHUNK + heightmapXY.x + 1];

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
            const f32 br = heightData[heightmapXY.y * HEIGHTMAP_VERT_WIDTH_PER_CHUNK + heightmapXY.x + 1];
            const f32 tl = heightData[(heightmapXY.y + 1) * HEIGHTMAP_VERT_WIDTH_PER_CHUNK + heightmapXY.x];
            const f32 tr = heightData[(heightmapXY.y + 1) * HEIGHTMAP_VERT_WIDTH_PER_CHUNK + heightmapXY.x + 1];

            const f32v3 uvw = BarycentricBrTlTr(dxy);
            return br * uvw.x + tl * uvw.y + tr * uvw.z;
        }
        else {
            // Lower quadrant
            // Get the 3 corner heights
            const f32 bl = heightData[heightmapXY.y * HEIGHTMAP_VERT_WIDTH_PER_CHUNK + heightmapXY.x];
            const f32 br = heightData[heightmapXY.y * HEIGHTMAP_VERT_WIDTH_PER_CHUNK + heightmapXY.x + 1];
            const f32 tl = heightData[(heightmapXY.y + 1) * HEIGHTMAP_VERT_WIDTH_PER_CHUNK + heightmapXY.x];

            const f32v3 uvw = BarycentricBlBrTl(dxy);
            return bl * uvw.x + br * uvw.y + tl * uvw.z;
        }
    }
}

HeightmapPatch::~HeightmapPatch() {
    delete[] mHeightData;
}
