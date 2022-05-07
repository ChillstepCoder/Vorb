#pragma once

#include "world/TileConst.h"
#include "tile/TileFlags.h"

class Building;

struct StructureFloor {
    ui32 basePosition;
    ui32 roofPosition;
};

class StructureTile {
    union {
        struct {
            TileID mIdWall;
            TileID mIdFloor;
            TileID mIdObject;
        };
        TileID mTileIDs[3] = { TILE_ID_NONE, TILE_ID_NONE, TILE_ID_NONE };
    };
    TileFlags flags;
    ui8 mPathWeight;
    // TODO: //ui16 navNodeIndex = UINT16_MAX; // Modified by nav thread only
};
static_assert(sizeof(StructureTile) == 8, "Keep small");

enum class StructureType : ui8 {
    OBSTACLE,
    BUILDING
};
// Store these in boost::rtree
class Structure {
    std::vector<StructureTile> mMainThreadTiles;
    std::vector<StructureTile> mThreadSafeTiles;
    std::vector<ui32> mFloorPositions;
    ui32AABB3 mAabb;
    std::vector<ui32> mTilesNeedingThreadUpdate;
    // TODO: Mutex?
    StructureType mType;
    union {
        Building* mBuildingStructureData;
    };
};

