#pragma once

#include "world/Region.h"
#include <Vorb/concurrentqueue.h>

#include "world/TerrainConstants.h"
#include "util/IntersectionHit.h"

class Camera3D;
class World;

constexpr ui32 HEIGHTMAP_QUAD_WIDTH_PER_CHUNK = CHUNK_WIDTH / HEIGHTMAP_QUAD_SIZE;
constexpr ui32 HEIGHTMAP_VERT_WIDTH_PER_CHUNK = HEIGHTMAP_QUAD_WIDTH_PER_CHUNK + 1;
constexpr ui32 HEIGHTMAP_VERT_SIZE_PER_CHUNK = SQ(HEIGHTMAP_VERT_WIDTH_PER_CHUNK);

enum HeightmapPatchFlags : ui32 {
    HEIGHTMAP_PATCH_FLAG_GENERATING = 1 << 0,
    HEIGHTMAP_PATCH_FLAG_DONE = 1 << 1
};

struct HeightmapPatchData {
    f32 data[HEIGHTMAP_VERT_SIZE_PER_CHUNK];
    BoundingSphere boundingSphere;
    f32AABB3 aabb;
};

enum class TerrainHeightSetDirection {
    ANY,
    RAISE,
    LOWER
};

class HeightmapPatch {
public:
    HeightmapPatch() = default;
    ~HeightmapPatch();

    bool isDone() const { return mFlags & HEIGHTMAP_PATCH_FLAG_DONE; }
    bool isGenerating() const { return mFlags & HEIGHTMAP_PATCH_FLAG_GENERATING; }

    ui32 mFlags = 0u;
    ui32 mRefCount = 0u;
    HeightmapPatchData* mHeightData = nullptr;
};
static_assert(sizeof(HeightmapPatch) == 16, "Keep small");

// Contains chunks and height data
class WorldGrid {
public:
    WorldGrid(World& world);

    Chunk& getChunk(ui32 i) { return mChunks[i]; }
    const Chunk& getChunk(ui32 i) const { return mChunks[i]; }
    Chunk& getChunk(ChunkID id) { return mChunks[id.id]; }
    const Chunk& getChunk(ChunkID id) const { return mChunks[id.id]; }
    
    static ui32 numChunks() { return WorldData::WORLD_SIZE_CHUNKS; }

    void requestHeightDataGenAndAquireAt(ChunkID id, std::function<void()> callback);
    const HeightmapPatchData* getHeightDataAt(ChunkID id) const;
    const HeightmapPatchData* tryGetHeightDataAt(ChunkID id) const;
    const HeightmapPatchData* aquireHeightData(ChunkID id);
    void releaseHeightDataAt(ChunkID id);
    void setHeightAt(f32v2 worldPos, f32 height, TerrainHeightSetDirection dir = TerrainHeightSetDirection::ANY);
    void setHeightAt(ChunkID id, ui32 vertIndex, f32 height, TerrainHeightSetDirection dir = TerrainHeightSetDirection::ANY);
    void adjustHeightAt(ChunkID id, ui32 vertIndex, f32 adjust);
    void flattenAABB(const ui32AABB2& aabb, f32 flattenHeight);

    f32 getHeightAtVert(ChunkID id, const ui32v2& vertPos) const;
    bool tryComputeHeightAtPoint(const f32v2& worldPos, f32* h) const;

    TerrainPickData pickTerrainFromCameraVector(const Camera3D& camera, const f32v3& rayDir) const;

    static f32 computeHeightAtPoint(ChunkID id, const f32* heightData, const f32v2& worldPos);
    static f32 computeHeightAtChunkOffset(const f32* heightData, const f32v2& chunkOffset);
    static f32 computeCenterHeightAtTile(const f32* heightData, TileIndex tileIndex);

    static f32 computeMinHeightAtTile(const f32* heightData, TileIndex tileIndex);

private:
    void setHeightAtInternal(ChunkID id, ui32 vertIndex, f32 height, TerrainHeightSetDirection dir);

    static f32 interpolateHeightAtOffset(f32v2 dxy, const f32* heightData, const ui32v2& heightmapXY);

    HeightmapPatch mHeightData[WorldData::WORLD_SIZE_CHUNKS];
    Chunk mChunks[WorldData::WORLD_SIZE_CHUNKS];
    std::vector<ui32> mActiveHeightmapPatches;
    World& mWorld;

    std::map<ChunkID, std::list<std::function<void()>>> mFinishCallbacks; // Runs when generation is finished
    
};