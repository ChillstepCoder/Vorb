#pragma once

#include "tile/TileConst.h"
#include "tile/TileFlags.h"
#include "tile/TileContainer.h"
#include "tile/TileHandle.h"

#include "util/BitArray.h"

enum class StructureType {
    Building
};


typedef ui32 StructureID;

class Structure {
    friend class StructureManager;
public:
    Structure() = default;
    virtual ~Structure() = default;

    const i32AABB3& getAABB() const { return mAABB; }

    StructureType getType() const { return mType; }
    TileContainer& getTileContainer() { return mTileContainer; }
    const TileContainer& getTileContainer() const { return mTileContainer; }
    i32v3 getWorldPositionOfTile(TileIndex tile) const;

    bool isTileOwned(TileIndex index) const { return mInteriorTilesInAABB.getBit(index); }


protected:
    TileContainer mTileContainer;
    BitArray mInteriorTilesInAABB;
    i32AABB3 mAABB;
    //f32 mZPosFloor;
    StructureType mType = StructureType::Building; // TODO: Different types?
    StructureID mId;
};

