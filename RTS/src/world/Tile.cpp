#include "stdafx.h"
#include "world/Tile.h"

#include "world/TileRepository.h"


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

bool Tile::canAddTile(const TileData& tile) const {
    return layers[tile.layer] == TILE_ID_NONE;
}

void Tile::addTile(const TileData& tile) {
    layers[tile.layer] = tile.id;
}

bool Tile::tryAddTile(const TileData& tile) {
    if (!canAddTile(tile)) {
        return false;
    }
    addTile(tile);
}

const TileCollider* Tile::tryGetCollider() const {
    if (tileFlags & TILE_FLAG_HAS_COLLIDER) {
        assert(topLayer != TILE_ID_NONE);
        return &TileRepository::getTileData(topLayer).collider;
    }
    return nullptr;
}
