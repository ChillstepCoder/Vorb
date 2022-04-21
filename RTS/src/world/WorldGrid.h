#pragma once

#include "world/Region.h"
#include <Vorb/concurrentqueue.h>

#include "world/TerrainConstants.h"
#include "util/IntersectionHit.h"

class Camera3D;
class World;

enum HeightmapPatchFlags : ui32 {
    HEIGHTMAP_PATCH_FLAG_GENERATING = 1 << 0,
    HEIGHTMAP_PATCH_FLAG_DONE = 1 << 1
};

struct HeightmapPatchData {
    HeightmapPatchData(HeightmapPatchID id) : id(id) {};

    f32 data[HEIGHTMAP_VERT_SIZE_PER_PATCH];
    BoundingSphere boundingSphere;
    f32AABB3 aabb;
    HeightmapPatchID id;
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

    void requestHeightDataGenAndAquireAt(HeightmapPatchID id, std::function<void()> callback);

    void requestPaddedHeightDataGenAndAquireAt(HeightmapPatchID id, std::function<void()> callback);

    const HeightmapPatchData* getHeightDataAt(HeightmapPatchID id) const;
    const HeightmapPatchData* tryGetHeightDataAt(HeightmapPatchID id) const;
    const HeightmapPatchData* aquireHeightData(HeightmapPatchID id);
    bool tryAquirePaddedHeightDataAt(HeightmapPatchID id);
    void getPaddedHeightDataAt(HeightmapPatchID id, OUT const HeightmapPatchData* paddedHeightData[9]);
    void releaseHeightDataAt(HeightmapPatchID id);
    void releasePaddedHeightDataAt(HeightmapPatchID id);
    void setHeightAt(f32v2 worldPos, f32 height, TerrainHeightSetDirection dir = TerrainHeightSetDirection::ANY);
    void setHeightAt(ChunkID id, ui32 vertIndex, f32 height, TerrainHeightSetDirection dir = TerrainHeightSetDirection::ANY);
    void setHeightAt(HeightmapPatchID patchId, ui32 vertIndex, f32 height, TerrainHeightSetDirection dir = TerrainHeightSetDirection::ANY);
    void adjustHeightAt(ChunkID id, ui32 vertIndex, f32 adjust);
    void adjustHeightAt(HeightmapPatchID id, ui32 vertIndex, f32 adjust);
    void flattenAABB(const ui32AABB2& aabb, f32 flattenHeight);

    f32 getHeightAtVert(ChunkID id, const ui32v2& vertPos) const;
    bool tryComputeHeightAtPoint(const f32v2& worldPos, f32* h) const;
    f32 tryComputeHeightAtPoint(const f32v2& worldPos) const;

    TerrainPickData pickTerrainFromCameraVector(const Camera3D& camera, const f32v3& rayDir) const;

    static f32 computeHeightAtPoint(HeightmapPatchID id, const f32* heightData, const f32v2& worldPos);
    static f32 computeHeightAtChunkOffset(const f32* heightData, ChunkID chunkId, const f32v2& chunkOffset);
    static f32 computeCenterHeightAtTile(const f32* heightData, TilePosition tilePos);
    static void computeTileCorners(const f32* heightData, TilePosition tilePos, OUT f32 corners[4]);
    static bool areTrianglesFlippedAtTile(TileIndex tileIndex);
    f32 computeCenterHeightAtTile(TilePosition tilePos) const;
    void copyHeightRowToBuffer(f32* dst, ui32v2 worldPosStart, ui32 rowLength) const;

    static f32 computeMinHeightAtTile(const f32* heightData, TilePosition tilePos);
    f32 computeMinHeightAtTile(TilePosition tilePos) const;
    f32 computeMaxHeightAtTile(TilePosition tilePos) const;

private:
    void generateHeightDataPatch(HeightmapPatch& patch, const f32v2& position);
    void onPatchFinishedGenerating(HeightmapPatchID id);
    void setHeightAtInternal(HeightmapPatchID id, ui32 vertIndex, f32 height, TerrainHeightSetDirection dir);
    void computeRequiredPaddedIDs(HeightmapPatchID id, OUT HeightmapPatchID requiredIds[9]) const;

    static f32 interpolateHeightAtOffset(f32v2 dxy, const f32* heightData, const ui32v2& heightmapXY);
    static ui32v2 getHeightmapXYfromTilePos(TilePosition tilePos);
    static f32v2 getHeightmapOffsetFromTilePos(TilePosition tilePos);

    HeightmapPatch mHeightData[WORLD_SIZE_HEIGHTMAP_PATCHES];
    Chunk mChunks[WorldData::WORLD_SIZE_CHUNKS];
    std::vector<ui32> mActiveHeightmapPatches;
    World& mWorld;

    std::map<ui32, std::list<std::function<void()>>> mFinishCallbacks; // Runs when generation is finished
    std::map<ui32, std::list<std::function<void()>>> mPaddedFinishCallbacks; // Runs when generation is finished
    std::map<ui32, ui32> mPaddedGenWaitCount;
    std::map<ui32, std::vector<HeightmapPatchID>> mPaddedGenListeners; // A list of listeners waiting for generation of a heightmap id
    
};