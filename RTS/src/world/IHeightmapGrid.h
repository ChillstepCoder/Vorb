#pragma once

#include <Vorb/concurrentqueue.h>

#include "terrain/HeightmapPatch.h"
#include "world/TerrainConstants.h"

#include <boost/container/flat_set.hpp>

#include "util/SpatialGrid2D.h"

#include "world/ChunkGridEvent.h"

#include <mutex>

class World;
class BitArray;
struct TileHandle;

enum class TerrainHeightSetDirection {
    ANY,
    RAISE,
    LOWER
};

//class HeightmapPatchHandleData {
//    friend class IHeightmapGrid;
//public:
//    HeightmapPatchHandleData();
//    ~HeightmapPatchHandleData();
//
//    VORB_NON_COPYABLE_BUT_MOVABLE(HeightmapPatchHandleData);
//
//    void* operator new(size_t count);
//    void operator delete(void* pointer, size_t size);
//
//private:
//    HeightmapPatch* patch = nullptr;
//    std::atomic_bool isFinishedGenerating = false;
//};
//
//typedef std::unique_ptr<HeightmapPatchHandleData> HeightmapPatchHandle;
//
//typedef void(*HeightmapRequestFunction)(HeightmapPatchHandleData&& handle);

enum class HeightmapGridEventType {
    EditVerts
};
struct HeightmapGridEvent {
    HeightmapGridEventType mEventType;
    const boost::container::flat_set<i32v2>* mModifiedVerts = nullptr;
};
EVENT_DISPATCHER_TYPE(IHeightmapGrid, HeightmapGridEventType, const HeightmapGridEvent&);

struct HeightmapPickResult {
    f32v3 hitPoint;
    f32v3 hitNormal;
    f32 hitTime = 1.0f; // [0, 1]
    bool didHit = false;
};

// TODO: Lookat alternate mountain gen https://www.youtube.com/watch?v=gsJHzBTPG0Y
// Game thread can read lockless but must lock to write.
// Other threads must use thread-safe template argument
class IHeightmapGrid
{
    friend class WorldSaveContext;
public:
    IHeightmapGrid(i32 worldWidthTiles);
    ~IHeightmapGrid();

    VORB_NON_COPYABLE(IHeightmapGrid);

    void onWorldBegin();

    void tickShared();

    // Picking
    HeightmapPickResult pick(f32v3 rayStart, f32v3 rayEnd);

    // Aquire
    const HeightmapPatch* getHeightDataAtWorldPos(const i32v2 worldPos) const;
    const HeightmapPatch* getHeightDataAt(HeightmapPatchID id) const;
    HeightmapPatch& getPatchForGeneration(HeightmapPatchID id);
    void getPaddedHeightDataAt(HeightmapPatchID id, OUT const HeightmapPatch* paddedHeightData[9]);

    // Mutators
    void setHeightAtWorldPos(f32v2 worldPos, f32 height, TerrainHeightSetDirection dir = TerrainHeightSetDirection::ANY);
    void setHeightAtChunkId(ChunkID id, i32 vertIndex, f32 height, TerrainHeightSetDirection dir = TerrainHeightSetDirection::ANY);
    void setHeightAtPatch(HeightmapPatchID patchId, i32 vertIndex, f32 height, TerrainHeightSetDirection dir = TerrainHeightSetDirection::ANY);
    void adjustHeightAtChunk(ChunkID id, i32 vertIndex, f32 adjust);
    void adjustHeightAtPatch(HeightmapPatchID id, i32 vertIndex, f32 adjust);
    void markVertexDirty(HeightmapPatchID id, i32 vertIndex);
    void flattenAABB(const i32AABB2& aabb, f32 flattenHeight);

    template<bool THREAD_SAFE>
    f32 getHeightAtVert(DTileCoord vertPos) const;
    f32 getHeightAtVert(HeightmapPatchID id, DTileCoord vertPos) const;

    template<bool THREAD_SAFE>
    CompressedHeight getCompressedHeightAtVert(DTileCoord vertPos) const;

    template <bool THREAD_SAFE>
    f32 computeHeightAtPoint(const f32v2 worldPos) const;
    // Never thread safe
    f32 computeHeightAtPointForGeneration(const f32v2 worldPos) const;
    // Each vertex spans 2 tiles
    f32 getHeightAtVertexForGeneration(const i32v2 worldVertexOffset) const;

    template <bool THREAD_SAFE>
    f32 computeHeightAndNormalAtPoint(const f32v2 worldPos, OUT f32v3* outNormal) const;

    template <bool THREAD_SAFE>
    f32 computeCenterHeightAtTile(TileCoord worldTilePos) const;

    template <bool THREAD_SAFE>
    f32 computeCenterHeightAndNormalAtTile(TileCoord worldTilePos, OUT f32v3* outNormal) const;

    void computeTileCorners(TileCoord worldTilePos, OUT f32 corners[4]) const;
    bool areTrianglesFlippedAtTile(const TileHandle& tileHandle) const;

    f32 computeMinHeightAtTile(TileCoord worldTilePos) const;
    f32 computeMaxHeightAtTile(TileCoord worldTilePos) const;

    f32 computeMeanHeightAtAABB(const i32AABB2& aabb) const;
    f32 computeMeanHeightAtAABB(const i32AABB2& aabb, const BitArray& checkBits) const;

    const SpatialGrid2D& getSpatialGrid2D() const { return mSpatialGrid2D; }
    World& getWorld() const { return *mWorld; }
    void setWorld(World& world) { mWorld = &world; }
    f32 getPatchWidthTiles() const { return HEIGHTMAP_PATCH_WIDTH_TILES; }
    f32 getPatchWidthVerts() const { return HEIGHTMAP_VERT_WIDTH_PER_PATCH; }
    i32 getWidthPatches() const { return mWidthPatches; }
    i32 getTotalPatches() const { return SQ(mWidthPatches); }

    EVENT_LISTENER_FUNCS(IHeightmapGrid, EditVerts, HeightmapGridEventType::EditVerts, const HeightmapGridEvent&);

protected:
    void initInternal();
    void initChunkGridEvents();
    void setHeightAtInternal(HeightmapPatchID id, i32 vertIndex, f32 height, TerrainHeightSetDirection dir);
    void computeRequiredPaddedIDs(HeightmapPatchID id, OUT HeightmapPatchID requiredIds[9]) const;

    template <bool THREAD_SAFE>
    f32 interpolateHeightAndNormalAtWorldPos(f32v2 worldPos, OUT f32v3* outNormal) const;
    template <bool THREAD_SAFE>
    f32 interpolateHeightAtWorldPos(f32v2 worldPos) const;

    SpatialGrid2D mSpatialGrid2D;
    std::unique_ptr<HeightmapPatch[]> mHeightData;
    i32 mWidthPatches = 0;
    i32 mTotalPatches;
    f32 mMaxCoordinate;

    World* mWorld = nullptr;

    ChunkGridListeners mChunkGridListeners;
    //std::mutex mMutex;

    // TODO: Server only
    boost::container::flat_set<i32v2> mModifiedVertsThisTick;
    boost::container::flat_set<HeightmapPatchID> mModifiedPatchesThisTick;
    // Events
    EVENT_DISPATCHER_DEF(IHeightmapGrid);

    BINARY_SERIALIZE() {
        s.value4b(mWidthPatches);
        if (!mHeightData) {
            initInternal();
        }
        for (i32 i = 0; i < mTotalPatches; ++i) {
            s.object(mHeightData[i]);
        }
    }
};