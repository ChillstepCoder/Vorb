#pragma once
#include "world/Tile.h"

enum WorldObjectFlags {
    HasCollision = 1 << 0
};

class WorldObject {
public:

private:
    TileIndex mRootPosition;
    i8v2 mDims;
};

