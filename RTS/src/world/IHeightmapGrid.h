#pragma once

#include <Vorb/concurrentqueue.h>

#include "terrain/HeightmapPatch.h"
#include "world/TerrainConstants.h"

#include <boost/container/flat_set.hpp>
#include <boost/container/flat_map.hpp>

#include "util/SpatialGrid2D.h"

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

class IHeightmapGrid
{
    friend class TerrainGenerator;
public:
    IHeightmapGrid(ui32 worldWidthTiles);
    ~IHeightmapGrid();

    void tickShared();

    // Aquire
    const HeightmapPatchData* getHeightDataAtWorldPos(const i32v2& worldPos) const;
    const HeightmapPatchData* getHeightDataAt(HeightmapPatchID id) const;
    void getPaddedHeightDataAt(HeightmapPatchID id, OUT const HeightmapPatchData* paddedHeightData[9]);

    // Mutators
    void setHeightAtWorldPos(f32v2 worldPos, f32 height, TerrainHeightSetDirection dir = TerrainHeightSetDirection::ANY);
    void setHeightAtChunkId(ChunkID id, ui32 vertIndex, f32 height, TerrainHeightSetDirection dir = TerrainHeightSetDirection::ANY);
    void setHeightAtPatch(HeightmapPatchID patchId, ui32 vertIndex, f32 height, TerrainHeightSetDirection dir = TerrainHeightSetDirection::ANY);
    void adjustHeightAtChunk(ChunkID id, ui32 vertIndex, f32 adjust);
    void adjustHeightAtPatch(HeightmapPatchID id, ui32 vertIndex, f32 adjust);
    void flattenAABB(const i32AABB2& aabb, f32 flattenHeight);

    f32 getHeightAtVert(HeightmapPatchID id, const ui32v2& vertPos) const;
    f32 computeHeightAtPoint(const f32v2& worldPos) const;
    f32 computeHeightAndNormalAtPoint(const f32v2& worldPos, OUT f32v3* outNormal) const;

    f32 getHeightAtPointThreadSafe(const f32v2& worldPos) const;
    f32 getHeightAndNormalAtPointThreadSafe(const f32v2& worldPos, OUT f32v3* outNormal) const;

    f32 computeHeightAtChunkOffset(const CompressedHeight* heightData, ChunkID chunkId, const f32v2& offsetIntoChunk);
    f32 computeHeightAtPoint(const CompressedHeight* heightData, const f32v2& worldPos) const;
    f32 computeHeightAtPoint(HeightmapPatchID id, const CompressedHeight* heightData, const f32v2& worldPos) const;
    f32 computeHeightAndNormalAtPoint(HeightmapPatchID id, const CompressedHeight* heightData, const f32v2& worldPos, OUT f32v3* outNormal) const;
    f32 computeCenterHeightAtTile(const CompressedHeight* heightData, ui32v2 worldTilePos) const;
    void computeTileCorners(const CompressedHeight* heightData, ui32v2 worldTilePos, OUT f32 corners[4]) const;
    bool areTrianglesFlippedAtTile(const TileHandle& tileHandle) const;
    f32 computeCenterHeightAtTile(ui32v2 worldTilePos) const;
    void copyHeightRowToBuffer(CompressedHeight* dst, i32v2 worldPosStart, ui32 rowLength) const;

    f32 computeMinHeightAtTile(const CompressedHeight* heightData, ui32v2 worldTilePos) const;
    f32 computeMinHeightAtTile(ui32v2 worldTilePos) const;
    f32 computeMaxHeightAtTile(ui32v2 worldTilePos) const;

    f32 computeMeanHeightAtAABB(const i32AABB2& aabb) const;
    f32 computeMeanHeightAtAABB(const i32AABB2& aabb, const BitArray& checkBits) const;

    const SpatialGrid2D& getSpatialGrid2D() const { return mSpatialGrid2D; }
    World& getWorld() const { return *mWorld; }
    void setWorld(World& world) { mWorld = &world; }

    EVENT_LISTENER_FUNCS(IHeightmapGrid, EditVerts, HeightmapGridEventType::EditVerts, const HeightmapGridEvent&);


protected:
    void onPatchFinishedGenerating(HeightmapPatchID id);
    void setHeightAtInternal(HeightmapPatchID id, ui32 vertIndex, f32 height, TerrainHeightSetDirection dir);
    void computeRequiredPaddedIDs(HeightmapPatchID id, OUT HeightmapPatchID requiredIds[9]) const;

    static f32 getHeightAndNormalAtOffset(f32v2 dxy, const CompressedHeight* heightData, const ui32v2 heightmapXY, OUT f32v3* outNormal);
    static f32 interpolateHeightAtOffset(f32v2 dxy, const CompressedHeight* heightData, const ui32v2& heightmapXY);
    static ui32v2 getHeightmapXYfromTilePos(ui32v2 worldTilePos);
    static f32v2 getHeightmapOffsetFromTilePos(ui32v2 worldTilePos);

    SpatialGrid2D mSpatialGrid2D;
    std::unique_ptr<HeightmapPatch[]> mHeightData;
    ui32 mWidthPatches;
    ui32 mTotalPatches;
    f32 mMaxCoordinate;

    std::map<ui32, std::list<std::function<void()>>> mFinishCallbacks; // Runs when generation is finished
    std::map<ui32, std::list<std::function<void()>>> mPaddedFinishCallbacks; // Runs when generation is finished
    std::map<ui32, ui32> mPaddedGenWaitCount;
    std::map<ui32, std::vector<HeightmapPatchID>> mPaddedGenListeners; // A list of listeners waiting for generation of a heightmap id

    World* mWorld = nullptr;
    //std::mutex mMutex;

    // TODO: Server only
    boost::container::flat_set<i32v2> mModifiedVertsThisTick;
    // Events
    EVENT_DISPATCHER_DEF(IHeightmapGrid);
};
