#pragma once

#include "tile/TileConst.h"
#include "tile/TileFlags.h"
#include "tile/TileContainer.h"
#include "tile/TileHandle.h"

#include "util/BitArray.h"

class BuildingBlueprint;

enum class StructureType : ui8 {
    Building
};

enum class StructureState : ui8 {
    ACTIVE,
    DEACTIVATING,
    SIM
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
    Building() = default;
    virtual ~Building() = default;

    Building(Building&& other) noexcept;
    Building& operator=(Building&& other) noexcept;

    VORB_NON_COPYABLE(Building);

    const i32AABB3& getTileAABB() const { return mTileAABB; }
    ui8 getFloorHeight() const { return mFloorHeight; }

    BuildingID getId() const { return mId; }
    StructureType getType() const { return mType; }
    TileContainer* getTileContainer() { return mTileContainer; }
    const TileContainer* getTileContainer() const { return mTileContainer; }

    const BitArray& getOwnedDTiles() const { return mOwnedDTiles; }
    bool isTileOwned(TileIndex index) const { return mOwnedDTiles.getBit(structureTileIndexToDTileIndex(index % (mTileAABB.width * mTileAABB.depth), mTileAABB.width >> 1)); }

    void incRef() { assert(mTileContainer); mTileContainer->incRef(); }
    void decRef() { assert(mTileContainer); mTileContainer->decRef(); }
    ui32 getRefCount() const { assert(mTileContainer); return mTileContainer->getRefCount(); }

    const ChunkID* getChunkDependencies() const { return mChunkDependencies; }
    ui32 getChunkDependencyCount() const { return mChunkdDependencyCount; }
    bool hasUnloadedChunkDependencies() const { return mChunkDependenciesSimulating != 0; }

    void setBlueprint(std::unique_ptr<BuildingBlueprint>&& bp);
    BuildingBlueprint* getBlueprint() const { return mBlueprint.get(); }

    void freeData();

protected:
    BitArray mOwnedDTiles;
    TileContainer* mTileContainer = nullptr;
    i32AABB3 mTileAABB;
    //f32 mZPosFloor;
    //ui32 mStateArrayIndex = UINT32_MAX;
    BuildingID mId = INVALID_STRUCTURE_ID;
    ChunkID mChunkDependencies[4] = { INVALID_STRUCTURE_ID,INVALID_STRUCTURE_ID,INVALID_STRUCTURE_ID,INVALID_STRUCTURE_ID };
    ui8 mChunkdDependencyCount : 4;
    ui8 mChunkDependenciesSimulating : 4;
    ui8 mFloorHeight;
    StructureType mType = StructureType::Building; // TODO: Different types?
    StructureState mState = StructureState::SIM;
    std::unique_ptr<BuildingBlueprint> mBlueprint; // If valid, building has not been serialized to disk

    // Entity owning this plot, can be a person or a business
    entt::entity mOwnerEntity = INVALID_ENTITY;
};
