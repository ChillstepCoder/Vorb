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
        tileFlags.setBit(TileFlags::TILE_FLAG_QUEUED_THREADSAFE_UPDATE);
    }
    else {
        tileFlagsThreadSafe.setBit(flag);
    }
    tileFlags.setBit(flag);
}

void Tile::setTileFlags(TileFlags flags, bool isReadLocked) {

    assert(IS_MAIN_THREAD()); 
    if (isReadLocked) {
        tileFlags.setBits(flags, TileFlags::TILE_FLAG_QUEUED_THREADSAFE_UPDATE);
    }
    else {
        tileFlagsThreadSafe = flags;
        tileFlags = flags;
    }
}

void Tile::clearTileFlag(TileFlags flag, bool isReadLocked) {

    assert(IS_MAIN_THREAD());
    if (isReadLocked) {
        tileFlags.setBit(TileFlags::TILE_FLAG_QUEUED_THREADSAFE_UPDATE);
    }
    else {
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

void Tile::clearTileCollisionFlags(bool isReadLocked) {

    assert(IS_MAIN_THREAD());

    tileFlags.clearMaskBits(TILE_COLLISION_FLAGS_MASK);
    if (isReadLocked) {
        tileFlags.setBit(TileFlags::TILE_FLAG_QUEUED_THREADSAFE_UPDATE);
    }
    else {
        tileFlagsThreadSafe = tileFlags;
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
    pathWeightThreadSafe = pathWeight;
}

bool Tile::canAddTile(const TileData& tile) const {
    assert(IS_MAIN_THREAD());
    return layers[tile.layer] == TILE_ID_NONE;
}

void Tile::addTile(const TileData& tile, bool isReadLocked) {
    assert(IS_MAIN_THREAD());
    if (isReadLocked) {
        tileFlags.setBit(TileFlags::TILE_FLAG_QUEUED_THREADSAFE_UPDATE);
    } else {
        layersThreadSafe[tile.layer] = tile.id;
    }
    layers[tile.layer] = tile.id;
}

bool Tile::tryAddTile(const TileData& tile, bool isReadLocked) {
    if (!canAddTile(tile)) {
        return false;
    }
    addTile(tile, isReadLocked);
    return true;
}

void Tile::setTileLayer(TileLayer layer, TileID id, bool isReadLocked) {
    assert(IS_MAIN_THREAD());
    if (isReadLocked) {
        tileFlags.setBit(TileFlags::TILE_FLAG_QUEUED_THREADSAFE_UPDATE);
    }
    else {
        layersThreadSafe[e_cast(layer)] = id;
    }
    layers[e_cast(layer)] = id;
}

void Tile::setPathWeight(ui8 weight, bool isReadLocked) {
    assert(IS_MAIN_THREAD());
    if (isReadLocked) {
        tileFlags.setBit(TileFlags::TILE_FLAG_QUEUED_THREADSAFE_UPDATE);
    }
    else {
        pathWeightThreadSafe = weight;
    }
    pathWeight = weight;
}

void Tile::setGroundZPosition(f32 groundZPosition, bool isReadLocked) {
    groundZPositionCompressed = compressTileZPosition(groundZPosition);
    if (isReadLocked) {
        tileFlags.setBit(TileFlags::TILE_FLAG_QUEUED_THREADSAFE_UPDATE);
    }
    else {
        groundZPositionCompressedThreadSafe = groundZPositionCompressed;
    }
}

void Tile::updateCollision(bool isReadLocked) {
    // Clear collision flags
    tileFlags.setBit((TileFlags)TILE_COLLISION_FLAGS_MASK);

    if (topLayer == TILE_ID_NONE) {
        tileFlags.clearBit(TileFlags::TILE_FLAG_HAS_COLLIDER);
    }
    else {
        const TileCollider& collider = TileRepository::getTileData(topLayer).collider;
        if (collider.isValid()) {
            tileFlags.setBit(collider.defaultFlags);
        }
        else {
            tileFlags.clearBit(TileFlags::TILE_FLAG_HAS_COLLIDER);
        }
    }

    if (isReadLocked) {
        tileFlags.setBit(TileFlags::TILE_FLAG_QUEUED_THREADSAFE_UPDATE);
    }
    else {
        tileFlagsThreadSafe = tileFlags;
    }
}

const TileCollider* Tile::tryGetColliderMainThread() const {
    assert(IS_MAIN_THREAD());
    if (tileFlags.isBitSet(TileFlags::TILE_FLAG_HAS_COLLIDER)) {
        assert(topLayer != TILE_ID_NONE);
        return &TileRepository::getTileData(topLayer).collider;
    }
    return nullptr;
}

const TileCollider* Tile::tryGetColliderThreadSafe() const {
    assert(!IS_MAIN_THREAD());
    if (tileFlagsThreadSafe.isBitSet(TileFlags::TILE_FLAG_HAS_COLLIDER)) {
        assert(topLayerThreadSafe != TILE_ID_NONE);
        return &TileRepository::getTileData(topLayerThreadSafe).collider;
    }
    return nullptr;
}