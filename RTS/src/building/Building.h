#pragma once

#include "tile/TileConst.h"
#include "tile/TileFlags.h"
#include "tile/TileContainer.h"
#include "tile/TileHandle.h"

#include "util/BitArray.h"

class BuildingBlueprint;

enum class BuildingState : ui8 {
    SIM,
    WAITING_SIM_RELEASE, // LAST SIM STATE
    LOADING_TILES,
    ACTIVE,
    INVALID
};

class StructureSimulationData {
public:
    BuildingID mId;
    i32AABB3 mAABB;
};

// Guarentees one structure can have no more than 4 chunk dependencies
const ui32 MAX_BUILDING_WIDTH_DTILES = CHUNK_WIDTH_DTILES - 1;

// A structure is a type of tile grid that has a base footpring and a number of floors.
// No two structures can have overlapping tiles.
class Building {
    friend class BuildingGrid;
public:
    Building();
    virtual ~Building();

    VORB_NON_COPYABLE(Building);

    const i32AABB3& getTileAABB() const { return mTileAABB; }
    ui8 getFloorHeight() const { return mFloorHeight; }
    i32 getFloorStride() const { return mTileAABB.dims.y * mTileAABB.dims.x; }

    BuildingID getId() const { return mId; }
    TileContainer* getTileContainer() { return mTileContainer; }
    const TileContainer* getTileContainer() const { return mTileContainer; }

    const BitArray& getOwnedDTiles() const { return mOwnedDTiles; }
    bool isTileOwned(TileIndex index) const { return mOwnedDTiles.getBit(structureTileIndexToDTileIndex(index % (mTileAABB.width * mTileAABB.depth), mTileAABB.width >> 1)); }

    void incRefTiles() { ASSERT_GAME_THREAD(); assert(mTileContainer); mTileContainer->incRef(); }
    void decRefTiles() { ASSERT_GAME_THREAD(); assert(mTileContainer); mTileContainer->decRef(); }
    ui32 getRefCountTiles() const { ASSERT_GAME_THREAD(); return mTileContainer ? mTileContainer->getRefCount() : 0; }

    const ChunkID* getChunkDependencies() const { return mChunkDependencies; }
    ui32 getChunkDependencyCount() const { return mChunkDependencyCount; }

    void setBlueprint(std::unique_ptr<BuildingBlueprint>&& bp);
    BuildingBlueprint* getBlueprint() const { return mBlueprint.get(); }

    void freeData();

    BuildingState getState() const { return mState.load(); }
    void setState(BuildingState state) { mState = state; }

    bool isValidForSimUsage() {
        ASSERT_SIM_THREAD();
        BuildingState state = mState.load();
        return (state == BuildingState::SIM || state == BuildingState::WAITING_SIM_RELEASE);
    }

    bool isValidForMainThreadUsage() {
        ASSERT_GAME_THREAD();
        return (mState == BuildingState::ACTIVE);
    }

    i32v3 getTileWorldPos(TileIndex index) const {
        i32 floorStride = getFloorStride();
        return mTileAABB.pos + i32v3(
            index % mTileAABB.width, (index / floorStride) / mTileAABB.width, (index / floorStride) * mFloorHeight
        );
    }

protected:
    BitArray mOwnedDTiles;
    TileContainer* mTileContainer = nullptr;
    i32AABB3 mTileAABB;
    //f32 mZPosFloor;
    //ui32 mStateArrayIndex = UINT32_MAX;
    BuildingID mId = INVALID_BUILDING_ID;
    ChunkID mChunkDependencies[4] = { INVALID_CHUNK_ID,INVALID_CHUNK_ID,INVALID_CHUNK_ID,INVALID_CHUNK_ID };
    ui8 mChunkDependencyCount : 4;
    ui8 mChunkDependenciesActiveCount : 4; // Main thread only
    ui8 mFloorHeight = 0;
    std::atomic<BuildingState> mState = BuildingState::INVALID;
    std::unique_ptr<BuildingBlueprint> mBlueprint; // If valid, building has not been serialized to disk
    bool mIsDeactivating = false;
    bool mIsDebugBuilding = false; // TODO: Bitflags?

    // Entity owning this plot, can be a person or a business
    entt::entity mOwnerEntity = INVALID_ENTITY;
};
