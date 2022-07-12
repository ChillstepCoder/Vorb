#pragma once

#include "tile/TileConst.h"
#include "tile/TileFlags.h"
#include "tile/TileContainer.h"

enum class StructureType {
    Building
};


typedef ui32 StructureID;


class Structure {
    friend class StructureManager;
public:
    Structure() = default;
    virtual ~Structure() = default;

    const ui32AABB3& getAABB() const { return mAABB; }

    StructureType getType() const { return mType; }
    TileContainer& getTileContainer() { return mTileContainer; }
    const TileContainer& getTileContainer() const { return mTileContainer; }
    ui32v3 getWorldPositionOfTile(TileIndex tile) const;


protected:
    TileContainer mTileContainer;
    ui32AABB3 mAABB;
    //f32 mZPosFloor;
    StructureType mType = StructureType::Building; // TODO: Different types?
    StructureID mId;
};

