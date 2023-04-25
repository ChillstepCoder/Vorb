#pragma once

#include <Vorb/concurrentqueue.h>

#include "terrain/HeightmapPatch.h"
#include "world/TerrainConstants.h"

#include <boost/container/flat_set.hpp>
#include <boost/container/flat_map.hpp>

#include "util/ThreadSafeDirtySet.h"

#include <mutex>

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
public:
    IHeightmapGrid();
    ~IHeightmapGrid();

    void tickShared();

    //// NEW INTERFACE
    //const HeightmapPatchData* tryGetHeightDataMainThread(HeightmapPatchID id) const;
    //void asyncGetPatchHandle(HeightmapPatchID id, OUT HeightmapPatchHandle& handle);
    //void releasePatchHandle(HeightmapPatchHandle&& handle);

    // Aquire
    void requestHeightDataGenAndAquireAt(HeightmapPatchID id, std::function<void()> callback);
    void requestPaddedHeightDataGenAndAquireAt(HeightmapPatchID id, std::function<void()> callback);
    const HeightmapPatchData* getHeightDataAt(HeightmapPatchID id) const;
    const HeightmapPatchData* tryGetHeightDataAt(HeightmapPatchID id) const;
    const HeightmapPatchData* aquireHeightData(HeightmapPatchID id);
    const HeightmapPatchData* tryAquireHeightData(HeightmapPatchID id);
    //const HeightmapPatchData* tryAquireHeightDataThreadSafe(HeightmapPatchID id);
    bool tryAquirePaddedHeightDataAt(HeightmapPatchID id);
    void getPaddedHeightDataAt(HeightmapPatchID id, OUT const HeightmapPatchData* paddedHeightData[9]);
    void releaseHeightDataAt(HeightmapPatchID id);
    void releasePaddedHeightDataAt(HeightmapPatchID id);

    // Mutators
    void setHeightAt(f32v2 worldPos, f32 height, TerrainHeightSetDirection dir = TerrainHeightSetDirection::ANY);
    void setHeightAt(ChunkID id, ui32 vertIndex, f32 height, TerrainHeightSetDirection dir = TerrainHeightSetDirection::ANY);
    void setHeightAt(HeightmapPatchID patchId, ui32 vertIndex, f32 height, TerrainHeightSetDirection dir = TerrainHeightSetDirection::ANY);
    void adjustHeightAt(ChunkID id, ui32 vertIndex, f32 adjust);
    void adjustHeightAt(HeightmapPatchID id, ui32 vertIndex, f32 adjust);
    void flattenAABB(const i32AABB2& aabb, f32 flattenHeight);

    f32 getHeightAtVert(HeightmapPatchID id, const ui32v2& vertPos) const;
    bool tryComputeHeightAtPoint(const f32v2& worldPos, f32* h) const;
    f32 tryComputeHeightAtPoint(const f32v2& worldPos) const;

    static f32 computeHeightAtPoint(HeightmapPatchID id, const f32* heightData, const f32v2& worldPos);
    static f32 computeHeightAtChunkOffset(const f32* heightData, ChunkID chunkId, const f32v2& chunkOffset);
    static f32 computeCenterHeightAtTile(const f32* heightData, ui32v2 worldTilePos);
    static void computeTileCorners(const f32* heightData, ui32v2 worldTilePos, OUT f32 corners[4]);
    static bool areTrianglesFlippedAtTile(const TileHandle& tileHandle);
    f32 computeCenterHeightAtTile(ui32v2 worldTilePos) const;
    void copyHeightRowToBuffer(f32* dst, i32v2 worldPosStart, ui32 rowLength) const;

    static f32 computeMinHeightAtTile(const f32* heightData, ui32v2 worldTilePos);
    f32 computeMinHeightAtTile(ui32v2 worldTilePos) const;
    f32 computeMaxHeightAtTile(ui32v2 worldTilePos) const;

    f32 computeMeanHeightAtAABB(const i32AABB2& aabb) const;
    f32 computeMeanHeightAtAABB(const i32AABB2& aabb, const BitArray& checkBits) const;

    STATIC_EVENT_LISTENER_FUNCS(IHeightmapGrid, EditVerts, HeightmapGridEventType::EditVerts, const HeightmapGridEvent&);

private:
    void generateHeightDataPatch(HeightmapPatch& patch, const f32v2& position);
    void onPatchFinishedGenerating(HeightmapPatchID id);
    void setHeightAtInternal(HeightmapPatchID id, ui32 vertIndex, f32 height, TerrainHeightSetDirection dir);
    void computeRequiredPaddedIDs(HeightmapPatchID id, OUT HeightmapPatchID requiredIds[9]) const;

    static f32 interpolateHeightAtOffset(f32v2 dxy, const f32* heightData, const ui32v2& heightmapXY);
    static ui32v2 getHeightmapXYfromTilePos(ui32v2 worldTilePos);
    static f32v2 getHeightmapOffsetFromTilePos(ui32v2 worldTilePos);

    HeightmapPatch mHeightData[WORLD_SIZE_HEIGHTMAP_PATCHES];
    std::vector<ui32> mActiveHeightmapPatches;

    std::map<ui32, std::list<std::function<void()>>> mFinishCallbacks; // Runs when generation is finished
    std::map<ui32, std::list<std::function<void()>>> mPaddedFinishCallbacks; // Runs when generation is finished
    std::map<ui32, ui32> mPaddedGenWaitCount;
    std::map<ui32, std::vector<HeightmapPatchID>> mPaddedGenListeners; // A list of listeners waiting for generation of a heightmap id
    //std::mutex mMutex;

    // TODO: Server only
    boost::container::flat_set<i32v2> mModifiedVertsThisTick;
    // Events
    STATIC_EVENT_DISPATCHER_DEF(IHeightmapGrid);
};

extern IHeightmapGrid* sHeightmapGrid;