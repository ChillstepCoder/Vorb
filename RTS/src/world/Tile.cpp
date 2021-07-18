#include "stdafx.h"
#include "world/Tile.h"

std::unordered_map<std::string, TileID> TileRepository::sTileIdMapping;
std::unordered_map<TileID, TileData> TileRepository::sTileData;

KEG_ENUM_DEF(TileShape, TileShape, kt) {
    kt.addValue("Floor", TileShape::FLOOR);
    kt.addValue("Thin", TileShape::THIN);
    kt.addValue("Thick", TileShape::THICK);
    kt.addValue("Roof", TileShape::ROOF);
}

KEG_ENUM_DEF(TileResource, TileResource, kt) {
    kt.addValue("none", TileResource::NONE);
    kt.addValue("wood", TileResource::WOOD);
    kt.addValue("stone", TileResource::STONE);
}

KEG_ENUM_DEF(TileCollisionShape, TileCollisionShape, kt) {
    kt.addValue("none", TileCollisionShape::FLOOR);
    kt.addValue("box", TileCollisionShape::BOX);
    kt.addValue("small_circle", TileCollisionShape::SMALL_CIRCLE);
    kt.addValue("medium_circle", TileCollisionShape::MEDIUM_CIRCLE);
}

KEG_TYPE_DEF_SAME_NAME(ItemDropDef, kt) {
    kt.addValue("item", keg::Value::basic(offsetof(ItemDropDef, itemName), keg::BasicType::STRING));
    kt.addValue("count", keg::Value::basic(offsetof(ItemDropDef, countRange), keg::BasicType::UI32_V2));
}

KEG_TYPE_DEF_SAME_NAME(TileData, kt) {
    kt.addValue("name", keg::Value::basic(offsetof(TileData, name), keg::BasicType::STRING));
    kt.addValue("tex", keg::Value::basic(offsetof(TileData, textureName), keg::BasicType::STRING));
    kt.addValue("col", keg::Value::custom(offsetof(TileData, collisionShape), "TileCollisionShape", true));
    kt.addValue("height", keg::Value::basic(offsetof(TileData, colliderHeight), keg::BasicType::F32));
    kt.addValue("path_weight", keg::Value::basic(offsetof(TileData, pathWeight), keg::BasicType::F32));
    kt.addValue("dims", keg::Value::basic(offsetof(TileData, dims), keg::BasicType::UI8_V2));
    kt.addValue("root", keg::Value::basic(offsetof(TileData, rootPos), keg::BasicType::UI8));
    kt.addValue("shape", keg::Value::custom(offsetof(TileData, shape), "TileShape", true));
    kt.addValue("resource", keg::Value::custom(offsetof(TileData, resource), "TileResource", true));
    kt.addValue("drops", keg::Value::array(offsetof(TileData, itemDrops), keg::Value::custom(0, "ItemDropDef", false)));
}

