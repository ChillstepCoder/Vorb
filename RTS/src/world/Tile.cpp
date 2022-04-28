#include "stdafx.h"
#include "world/Tile.h"

#include "world/TileRepository.h"

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
}

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
    if (isReadLocked) {
        tileFlags |= TILE_FLAG_QUEUED_UPDATE;
    }
    else {
        tileFlagsThreadSafe |= flag;
    }
    tileFlags |= flag;
}

void Tile::setTileFlags(TileFlags flags, bool isReadLocked) {

    assert(IS_MAIN_THREAD()); 
    if (isReadLocked) {
        tileFlags = flags | TILE_FLAG_QUEUED_UPDATE;
    }
    else {
        tileFlagsThreadSafe = flags;
        tileFlags = flags;
    }
}

void Tile::clearTileFlag(TileFlags flag, bool isReadLocked) {

    assert(IS_MAIN_THREAD());
    if (isReadLocked) {
        tileFlags |= TILE_FLAG_QUEUED_UPDATE;
    }
    else {
        tileFlagsThreadSafe &= (~flag);
    }
    tileFlags &= (~flag);
}

void Tile::clearTileFlags(bool isReadLocked) {

    assert(IS_MAIN_THREAD());
    if (isReadLocked) {
        tileFlags = TILE_FLAG_QUEUED_UPDATE;
    }
    else {
        tileFlagsThreadSafe = 0;
        tileFlags = 0;
    }
}

void Tile::clearTileCollisionFlags(bool isReadLocked) {

    assert(IS_MAIN_THREAD());

    tileFlags &= (~TILE_COLLISION_FLAGS_MASK);
    if (isReadLocked) {
        tileFlags |= TILE_FLAG_QUEUED_UPDATE;
    }
    else {
        tileFlagsThreadSafe = tileFlags;
    }
}

Tile::Tile(TileID ground, TileID mid, TileID top) {
    floors[TILE_FLOOR_GROUND].groundLayer = ground;
    floors[TILE_FLOOR_GROUND].midLayer = mid;
    floors[TILE_FLOOR_GROUND].topLayer = top;
}

Tile::Tile(TileID ground, TileID mid, TileID top, f32 zPos) : baseZPositionCompressed(compressTileZPosition(zPos)), baseZPositionCompressedThreadSafe(baseZPositionCompressed) {
    floors[TILE_FLOOR_GROUND].groundLayer = ground;
    floors[TILE_FLOOR_GROUND].midLayer = mid;
    floors[TILE_FLOOR_GROUND].topLayer = top;
}

Tile::Tile(TileID ground, TileID mid, TileID top, f32 zPos, TileFlags flags) : baseZPositionCompressed(compressTileZPosition(zPos)), baseZPositionCompressedThreadSafe(baseZPositionCompressed), tileFlags(flags), tileFlagsThreadSafe(flags) {
    floors[TILE_FLOOR_GROUND].groundLayer = ground;
    floors[TILE_FLOOR_GROUND].midLayer = mid;
    floors[TILE_FLOOR_GROUND].topLayer = top;
}

bool Tile::hasHarvestableResource(TileResource resource, TileLayer* outLayer) const {
    assert(IS_MAIN_THREAD());
    for (int i = 0; i < TILE_LAYER_COUNT; ++i) {
        TileID tileId = floors[TILE_FLOOR_GROUND].layers[i];
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
    assert(tileFlags & TILE_FLAG_QUEUED_UPDATE);
    tileFlags &= (~TILE_FLAG_QUEUED_UPDATE);

    tileFlagsThreadSafe = tileFlags;
    memcpy(floors[TILE_FLOOR_GROUND].layersThreadSafe, floors[TILE_FLOOR_GROUND].layers, sizeof(TileID) * TILE_LAYER_COUNT);
    baseZPositionCompressedThreadSafe = baseZPositionCompressed;
    pathWeightThreadSafe = pathWeight;
}

bool Tile::canAddTile(const TileData& tile) const {
    assert(IS_MAIN_THREAD());
    return floors[TILE_FLOOR_GROUND].layers[tile.layer] == TILE_ID_NONE;
}

void Tile::addTile(const TileData& tile, bool isReadLocked) {
    assert(IS_MAIN_THREAD());
    if (isReadLocked) {
        tileFlags |= TILE_FLAG_QUEUED_UPDATE;
    } else {
        floors[TILE_FLOOR_GROUND].layersThreadSafe[tile.layer] = tile.id;
    }
    floors[TILE_FLOOR_GROUND].layers[tile.layer] = tile.id;
}

bool Tile::tryAddTile(const TileData& tile, bool isReadLocked) {
    if (!canAddTile(tile)) {
        return false;
    }
    addTile(tile, isReadLocked);
    return true;
}

void Tile::setTileLayer(TileFloor floor, TileLayer layer, TileID id, bool isReadLocked) {
    assert(IS_MAIN_THREAD());
    if (isReadLocked) {
        tileFlags |= TILE_FLAG_QUEUED_UPDATE;
    }
    else {
        floors[floor].layersThreadSafe[e_cast(layer)] = id;
    }
    floors[floor].layers[e_cast(layer)] = id;
}

void Tile::setPathWeight(ui8 weight, bool isReadLocked) {
    assert(IS_MAIN_THREAD());
    if (isReadLocked) {
        tileFlags |= TILE_FLAG_QUEUED_UPDATE;
    }
    else {
        pathWeightThreadSafe = weight;
    }
    pathWeight = weight;
}

void Tile::setBaseZPosition(f32 baseZPosition, bool isReadLocked) {
    baseZPositionCompressed = compressTileZPosition(baseZPosition);
    if (isReadLocked) {
        tileFlags |= TILE_FLAG_QUEUED_UPDATE;
    }
    else {
        baseZPositionCompressedThreadSafe = baseZPositionCompressed;
    }
}

void Tile::updateCollision(bool isReadLocked) {
    // Clear collision flags
    tileFlags &= (~TILE_COLLISION_FLAGS_MASK);

    if (floors[TILE_FLOOR_GROUND].topLayer == TILE_ID_NONE) {
        if (tileFlags & TILE_FLAG_HAS_COLLIDER) {
            tileFlags &= ~(TILE_FLAG_HAS_COLLIDER);
        }
    }
    else {
        const TileCollider& collider = TileRepository::getTileData(floors[TILE_FLOOR_GROUND].topLayer).collider;
        if (collider.isValid()) {
            tileFlags |= collider.defaultFlags;
        }
        else if (tileFlags & TILE_FLAG_HAS_COLLIDER) {
            tileFlags &= ~(TILE_FLAG_HAS_COLLIDER);
        }
    }

    if (isReadLocked) {
        tileFlags |= TILE_FLAG_QUEUED_UPDATE;
    }
    else {
        tileFlagsThreadSafe = tileFlags;
    }
}

const TileCollider* Tile::tryGetColliderMainThread() const {
    assert(IS_MAIN_THREAD());
    if (tileFlags & TILE_FLAG_HAS_COLLIDER) {
        assert(floors[TILE_FLOOR_GROUND].topLayer != TILE_ID_NONE);
        return &TileRepository::getTileData(floors[TILE_FLOOR_GROUND].topLayer).collider;
    }
    return nullptr;
}

const TileCollider* Tile::tryGetColliderThreadSafe() const {
    assert(!IS_MAIN_THREAD());
    if (tileFlagsThreadSafe & TILE_FLAG_HAS_COLLIDER) {
        assert(floors[TILE_FLOOR_GROUND].topLayerThreadSafe != TILE_ID_NONE);
        return &TileRepository::getTileData(floors[TILE_FLOOR_GROUND].topLayerThreadSafe).collider;
    }
    return nullptr;
}