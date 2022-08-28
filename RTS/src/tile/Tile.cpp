#include "stdafx.h"
#include "tile/Tile.h"

#include "resources/TileRepository.h"

KEG_ENUM_DEF(TileTextureMethod, TileTextureMethod, kt) {
    kt.addValue("simple", TileTextureMethod::SIMPLE);
    kt.addValue("connected", TileTextureMethod::CONNECTED);
    kt.addValue("connected_wall", TileTextureMethod::CONNECTED_WALL);
    kt.addValue("vertical", TileTextureMethod::VERTICAL);
    kt.addValue("flora", TileTextureMethod::FLORA);
    kt.addValue("world_tiling", TileTextureMethod::WORLD_TILING);
}
static_assert(e_cast(TileTextureMethod::COUNT) == 6);

KEG_ENUM_DEF(TileShape, TileShape, kt) {
    kt.addValue("Thin", TileShape::THIN);
    kt.addValue("Block", TileShape::BLOCK);
    kt.addValue("Floor", TileShape::FLOOR);
    kt.addValue("Wall", TileShape::WALL);
    kt.addValue("Door", TileShape::DOOR);
    kt.addValue("Stairs", TileShape::STAIRS);
}
static_assert(e_cast(TileShape::COUNT) == 6);

KEG_ENUM_DEF(TileResource, TileResource, kt) {
    kt.addValue("none", TileResource::NONE);
    kt.addValue("wood", TileResource::WOOD);
    kt.addValue("stone", TileResource::STONE);
}

KEG_ENUM_DEF(TileCollisionShape, TileCollisionShape, kt) {
    kt.addValue("none", TileCollisionShape::NONE);
    kt.addValue("box", TileCollisionShape::BOX);
    kt.addValue("circle", TileCollisionShape::CIRCLE);
}

KEG_TYPE_DEF_SAME_NAME(ItemInputDef, kt) {
    kt.addValue("item", keg::Value::basic(offsetof(ItemInputDef, itemName), keg::BasicType::STRING));
    kt.addValue("count", keg::Value::basic(offsetof(ItemInputDef, count), keg::BasicType::UI32));
}

KEG_TYPE_DEF_SAME_NAME(ItemDropDef, kt) {
    kt.addValue("item", keg::Value::basic(offsetof(ItemDropDef, itemName), keg::BasicType::STRING));
    kt.addValue("count", keg::Value::basic(offsetof(ItemDropDef, countRange), keg::BasicType::UI32_V2));
}

void Tile::setTileFlag(TileFlags flag, bool isReadLocked) {

    assert(IS_MAIN_THREAD());
    if (!isReadLocked) {
        tileFlagsThreadSafe.setBit(flag);
    }
    tileFlags.setBit(flag);
}

void Tile::setTileFlags(TileFlags flags, bool isReadLocked) {

    assert(IS_MAIN_THREAD()); 
    if (!isReadLocked) {
        tileFlagsThreadSafe = flags;
        tileFlags = flags;
    }
}

void Tile::setOrientation(Cartesian dir, TileLayer layer, bool isReadLocked)
{
    switch (layer) {
        case TileLayer::Ground: {
            if (!isReadLocked) {
                orientationThreadSafe.orientationBase = dir;
            }
            orientation.orientationBase = dir;
            break;
        }
        case TileLayer::Mid: {
            if (!isReadLocked) {
                orientationThreadSafe.orientationMid = dir;
            }
            orientation.orientationMid = dir;
            break;
        }
        case TileLayer::Top: {
            if (!isReadLocked) {
                orientationThreadSafe.orientationTop = dir;
            }
            orientation.orientationTop = dir;
            break;
        }
    }
    static_assert(e_cast(TileLayer::COUNT) == 3);
}

void Tile::clearTileFlag(TileFlags flag, bool isReadLocked) {

    assert(IS_MAIN_THREAD());
    if (!isReadLocked) {
        tileFlagsThreadSafe.clearBit(flag);
    }
    tileFlags.clearBit(flag);
}

void Tile::clearTileFlags(bool isReadLocked) {

    assert(IS_MAIN_THREAD());
    if (isReadLocked) {
        tileFlags.overwriteBits(TileFlags::TILE_FLAG_QUEUED_THREADSAFE_UPDATE);
    }
    else {
        tileFlagsThreadSafe = 0;
        tileFlags = 0;
    }
}

Tile::Tile(TileID ground, TileID mid, TileID top) {
    groundLayer = ground;
    midLayer = mid;
    topLayer = top;
}

Tile::Tile(TileID ground, TileID mid, TileID top, f32 zPos) {
    groundLayer = ground;
    midLayer = mid;
    topLayer = top;
    groundZPositionCompressed = compressTileZPosition(zPos);
    // TODO: Do we need to update thread safe layers here?????
    // add TILE_FLAG_QUEUED_THREADSAFE_UPDATE??
}

Tile::Tile(TileID ground, TileID mid, TileID top, f32 zPos, TileFlags flags) : tileFlags(flags), tileFlagsThreadSafe(flags) {
    groundLayer = ground;
    midLayer = mid;
    topLayer = top;
    groundZPositionCompressed = compressTileZPosition(zPos);
    // TODO: Do we need to update thread safe layers here?????
    // add TILE_FLAG_QUEUED_THREADSAFE_UPDATE??
}

bool Tile::hasHarvestableResource(TileResource resource, TileLayer* outLayer) const {
    assert(IS_MAIN_THREAD());
    for (int i = 0; i < TILE_LAYER_COUNT; ++i) {
        // Harvestble resources only exist on ground floor
        TileID tileId = layers[i];
        if (tileId != INVALID_TILE_INDEX) {
            if (TileRepository::getTileData(tileId).resource == resource) {
                if (outLayer) {
                    *outLayer = (TileLayer)i;
                }
                return true;
            }
        }
    }
    return false;
}

void Tile::updateThreadSafeLayers() {
    assert(tileFlags.isBitSet(TileFlags::TILE_FLAG_QUEUED_THREADSAFE_UPDATE));
    tileFlags.clearBit(TileFlags::TILE_FLAG_QUEUED_THREADSAFE_UPDATE);

    tileFlagsThreadSafe = tileFlags;
    memcpy(layersThreadSafe, layers, sizeof(TileID) * TILE_LAYER_COUNT);
    groundZPositionCompressedThreadSafe = groundZPositionCompressed;
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

bool Tile::canNavInDirection(Cartesian8 dir) const {
    assert(!IS_MAIN_THREAD());
    if (midLayerThreadSafe == TILE_ID_NONE) return true;

    ui8 navMask = TileRepository::getTileData(midLayerThreadSafe).navMask;
    // South is base case
    // Rotate dir based on orientation to match the mask
    switch (orientationThreadSafe.orientationMid) {
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

const Cartesian& Tile::getOrientationMainThread(TileLayer layer) const {
    assert(IS_MAIN_THREAD());
    switch (layer) {
        case TileLayer::Ground: {
            return orientation.orientationBase;
        }
        case TileLayer::Mid: {
            return orientation.orientationMid;
        }
        case TileLayer::Top: {
            return orientation.orientationTop;
        }
    }
}

const Cartesian& Tile::getOrientationThreadSafe(TileLayer layer) const {
    assert(!IS_MAIN_THREAD());
    switch (layer) {
        case TileLayer::Ground: {
            return orientationThreadSafe.orientationBase;
        }
        case TileLayer::Mid: {
            return orientationThreadSafe.orientationMid;
        }
        case TileLayer::Top: {
            return orientationThreadSafe.orientationTop;
        }
    }
}

bool Tile::canAddTileData(const TileData& tile) const {
    assert(IS_MAIN_THREAD());
    return layers[tile.layer] == TILE_ID_NONE;
}

void Tile::addTileData(const TileData& tile, bool isReadLocked) {
    assert(IS_MAIN_THREAD());
    if (!isReadLocked) {
        layersThreadSafe[tile.layer] = tile.id;
    }
    layers[tile.layer] = tile.id;
}

void Tile::setTileLayer(TileLayer layer, TileID id, bool isReadLocked) {
    assert(IS_MAIN_THREAD());
    if (!isReadLocked) {
        layersThreadSafe[e_cast(layer)] = id;
    }
    layers[e_cast(layer)] = id;
}

void Tile::setGroundZPosition(f32 groundZPosition, bool isReadLocked) {
    groundZPositionCompressed = compressTileZPosition(groundZPosition);
    if (!isReadLocked) {
        groundZPositionCompressedThreadSafe = groundZPositionCompressed;
    }
}
//void Tile::setWall(Cartesian cartesianSouthOrWest, TileWall wall, bool isReadLocked) {
//    assert(e_cast(cartesianSouthOrWest) <= 1);
//    assert(IS_MAIN_THREAD());
//    if (isReadLocked) {
//        tileFlags.setBit(TileFlags::TILE_FLAG_QUEUED_THREADSAFE_UPDATE);
//    }
//    else {
//        wallsThreadSafe[e_cast(cartesianSouthOrWest)] = wall;
//    }
//    walls[e_cast(cartesianSouthOrWest)] = wall;
//}
