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
    DORMANT
};

class Structure {
    friend class StructureManager;
public:
    Structure() = default;
    virtual ~Structure() = default;

    const i32AABB3& getAABB() const { return mAABB; }

    StructureID getId() const { return mId; }
    StructureType getType() const { return mType; }
    TileContainer* getTileContainer() { return mTileContainer; }
    const TileContainer* getTileContainer() const { return mTileContainer; }
    i32v3 getWorldPositionOfTile(TileIndex tile) const;

    bool isTileOwned(TileIndex index) const { assert(mTileContainer);  return mTileContainer->isTileOwned(index); }

    void incRef() { assert(mTileContainer); mTileContainer->incRef(); }
    void decRef() { assert(mTileContainer); mTileContainer->decRef(); }

protected:
    TileContainer* mTileContainer = nullptr;
    i32AABB3 mAABB;
    //f32 mZPosFloor;
    //ui32 mStateArrayIndex = UINT32_MAX;
    StructureID mId = INVALID_STRUCTURE_ID;
    LiteChunkID mChunkDependencies[4];
    ui8 mChunkDependenciesUnloaded = 0;
    StructureType mType = StructureType::Building; // TODO: Different types?
    StructureState mState = StructureState::DORMANT;
    // TODO: LOD as well?
};
