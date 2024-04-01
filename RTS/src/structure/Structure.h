#pragma once

#include "tile/TileConst.h"
#include "tile/TileFlags.h"
#include "tile/TileContainer.h"
#include "tile/TileHandle.h"

#include "util/BitArray.h"

enum class StructureType : ui8 {
    Building
};

enum class StructureState : ui8 {
    ACTIVE,
    SIM
};

class StructureSimulationData {
public:
    StructureID mId;
    i32AABB3 mAABB;
};

// Guarentees one structure can have no more than 4 chunk dependencies
const ui32 MAX_STRUCTURE_WIDTH_DTILES = CHUNK_WIDTH_DTILES - 1;

// A structure is a type of tile grid that has a base footpring and a number of floors.
// No two structures can have overlapping tiles.
class Structure {
    friend class StructureGrid;
public:
    Structure() = default;
    virtual ~Structure() = default;

    const i32AABB3& getTileAABB() const { return mTileAABB; }

    StructureID getId() const { return mId; }
    StructureType getType() const { return mType; }
    TileContainer* getTileContainer() { return mTileContainer; }
    const TileContainer* getTileContainer() const { return mTileContainer; }

    const BitArray& getOwnedDTiles() const { return mOwnedDTiles; }
    bool isTileOwned(TileIndex index) const { return mOwnedDTiles.getBit(structureTileIndexToDTileIndex(index % (mTileAABB.width * mTileAABB.depth), mTileAABB.width >> 1)); }

    void incRef() { assert(mTileContainer); mTileContainer->incRef(); }
    void decRef() { assert(mTileContainer); mTileContainer->decRef(); }

    const ChunkID* getChunkDependencies() const { return mChunkDependencies; }
    bool hasUnloadedChunkDependencies() const { return mChunkDependenciesUnloaded != 0; }

protected:
    BitArray mOwnedDTiles;
    TileContainer* mTileContainer = nullptr;
    i32AABB3 mTileAABB;
    //f32 mZPosFloor;
    //ui32 mStateArrayIndex = UINT32_MAX;
    StructureID mId = INVALID_STRUCTURE_ID;
    ChunkID mChunkDependencies[4];
    ui8 mChunkDependenciesUnloaded = 0;
    StructureType mType = StructureType::Building; // TODO: Different types?
    StructureState mState = StructureState::SIM;
    // TODO: LOD as well?
};
