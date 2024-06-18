#include "stdafx.h"
#include "tile/Tile.h"

#include "resources/TileRepository.h"

void Tile::setTileFlag(TileFlags flag) {
    ASSERT_GAME_THREAD();
    tileFlags.setBit(flag);
}

void Tile::overwriteTileFlags(TileFlags flags) {
    ASSERT_GAME_THREAD(); 
    tileFlags = flags;
}

void Tile::setOrientation(Cartesian dir, TileLayer layer) {
    switch (layer) {
        case TileLayer::Ground: {
            orientation.orientationBase = dir;
            break;
        }
        case TileLayer::Main: {
            orientation.orientationMain = dir;
            break;
        }
    }
    static_assert(e_cast(TileLayer::COUNT) == 2);
}

void Tile::clearTileFlag(TileFlags flag) {
    ASSERT_GAME_THREAD();
    tileFlags.clearBit(flag);
}

void Tile::zeroTileFlags() {
    ASSERT_GAME_THREAD();
    tileFlags = 0;
}

Tile::Tile(TileID ground, TileID mid) {
    groundLayer = ground;
    mainLayer = mid;
}

Tile::Tile(TileID ground, TileID mid, f32 zPos) {
    groundLayer = ground;
    mainLayer = mid;
    groundZOffset = zPos;
    // TODO: Do we need to update thread safe layers here?????
    // add TILE_FLAG_QUEUED_THREADSAFE_UPDATE??
}

Tile::Tile(TileID ground, TileID mid, f32 zPos, TileFlags flags) : tileFlags(flags){
    groundLayer = ground;
    mainLayer = mid;
    groundZOffset = zPos;
    // TODO: Do we need to update thread safe layers here?????
    // add TILE_FLAG_QUEUED_THREADSAFE_UPDATE??
}

bool Tile::hasHarvestableResource(TileHarvestable resource, TileLayer* outLayer) const {
    ASSERT_GAME_THREAD();
    TileRepository& tileRepo = TileRepository::get();
    for (int i = 0; i < TILE_LAYER_COUNT; ++i) {
        // Harvestble resources only exist on ground floor
        TileID tileId = layers[i];
        if (tileId != TILE_ID_NONE) {
            if (tileRepo.getLoadedOrUnloadedAsset(tileId).harvestable == resource) {
                if (outLayer) {
                    *outLayer = (TileLayer)i;
                }
                return true;
            }
        }
    }
    return false;
}

// This was painful
// Describes how we map a cartesian8 to a rotated orientation (South is base case)
Cartesian8 ORIENTATION_ROTATE_DIR_WEST[8] = {
    Cartesian8::SOUTH_EAST, //SOUTH_WEST
    Cartesian8::EAST,  //SOUTH
    Cartesian8::NORTH_EAST,  //SOUTH_EAST
    Cartesian8::SOUTH, //WEST
    Cartesian8::NORTH, //EAST
    Cartesian8::SOUTH_WEST, //NORTH_WEST
    Cartesian8::WEST, //NORTH
    Cartesian8::NORTH_WEST, //NORTH_EAST
};

Cartesian8 ORIENTATION_ROTATE_DIR_NORTH[8] = {
    Cartesian8::NORTH_EAST, //SOUTH_WEST
    Cartesian8::NORTH,  //SOUTH
    Cartesian8::NORTH_WEST,  //SOUTH_EAST
    Cartesian8::EAST, //WEST
    Cartesian8::WEST, //EAST
    Cartesian8::SOUTH_EAST, //NORTH_WEST
    Cartesian8::SOUTH, //NORTH
    Cartesian8::SOUTH_WEST, //NORTH_EAST
};

Cartesian8 ORIENTATION_ROTATE_DIR_EAST[8] = {
    Cartesian8::NORTH_WEST, //SOUTH_WEST
    Cartesian8::WEST,  //SOUTH
    Cartesian8::SOUTH_WEST,  //SOUTH_EAST
    Cartesian8::NORTH, //WEST
    Cartesian8::SOUTH, //EAST
    Cartesian8::NORTH_EAST, //NORTH_WEST
    Cartesian8::EAST, //NORTH
    Cartesian8::SOUTH_EAST, //NORTH_EAST
};

// TODO: This needs to be copied?
bool Tile::canNavInDirection(Cartesian8 dir) const {
    assert(!IS_GAME_THREAD());
    if (mainLayer == TILE_ID_NONE) return true;
    ui8 navMask = TileRepository::get().getLoadedOrUnloadedAsset(mainLayer).navMask;
    // South is base case
    // Rotate dir based on orientation to match the mask
    switch (orientation.orientationMain) {
        case Cartesian::WEST:
            dir = ORIENTATION_ROTATE_DIR_WEST[e_cast(dir)];
            break;
        case Cartesian::EAST:
            dir = ORIENTATION_ROTATE_DIR_EAST[e_cast(dir)];
            break;
        case Cartesian::NORTH:
            dir = ORIENTATION_ROTATE_DIR_NORTH[e_cast(dir)];
            break;
    }
    return navMask & (1 << (ui8)dir);
}

f32 Tile::getEdgeHeightOffset(Cartesian dir) const {
    if (mainLayer == TILE_ID_NONE) return 0.0f;
    const TileDef& tileData = TileRepository::get().getLoadedOrUnloadedAsset(mainLayer);
    // TODO: Cut out this check
    Cartesian8 dir8 = CARTESIAN_TO_CARTESIAN8[e_cast(dir)];
    switch (orientation.orientationMain) {
        case Cartesian::WEST:
            dir8 = ORIENTATION_ROTATE_DIR_WEST[e_cast(dir8)];
            break;
        case Cartesian::EAST:
            dir8 = ORIENTATION_ROTATE_DIR_EAST[e_cast(dir8)];
            break;
        case Cartesian::NORTH:
            dir8 = ORIENTATION_ROTATE_DIR_NORTH[e_cast(dir8)];
            break;
    }
    dir = CARTESIAN8_TO_CARTESIAN[e_cast(dir8)];
    return tileData.heightOffsets[e_cast(dir)];
}

Cartesian Tile::getOrientation(TileLayer layer) const {
    switch (layer) {
        case TileLayer::Ground: {
            return orientation.orientationBase;
        }
        case TileLayer::Main: {
            return orientation.orientationMain;
        }
    }
}

bool Tile::canAddTileData(const TileDef& tile) const {
    ASSERT_GAME_THREAD();
    return layers[tile.layer] == TILE_ID_NONE;
}

void Tile::setGroundZOffset(f32 groundZPosition) {
    ASSERT_GAME_THREAD();
    groundZOffset = groundZPosition;
}
//void Tile::setWall(Cartesian cartesianSouthOrWest, TileWall wall, bool isReadLocked) {
//    assert(e_cast(cartesianSouthOrWest) <= 1);
//    ASSERT_GAME_THREAD();
//    if (isReadLocked) {
//        tileFlags.setBit(TileFlags::TILE_FLAG_QUEUED_THREADSAFE_UPDATE);
//    }
//    else {
//        wallsThreadSafe[e_cast(cartesianSouthOrWest)] = wall;
//    }
//    walls[e_cast(cartesianSouthOrWest)] = wall;
//}


// TODO: REMOVE
#include "math/Random.h"
f32 getTileModelRotationAtPosition(f32v2 worldPos) {
    return Random::getCachedRandomfSpecific((ui32)(worldPos.x + worldPos.y * 1000.0f)) * M_2_PI;
}