#pragma once

#include "tile/TileConst.h"
#include "tile/TileFlags.h"
#include "tile/TileContainer.h"

// Store these in boost::rtree?
class Structure {
protected:
    TileContainer mTileContainer;
    ui32AABB2 mAABB;
    f32 mZPosFloor;
};

