#include "stdafx.h"
#include "TileRepository.h"

std::unordered_map<std::string, TileID> TileRepository::sTileIdMapping;
std::vector<TileData> TileRepository::sTileData;

KEG_TYPE_DEF_SAME_NAME(TileData, kt) {
    kt.addValue("name", keg::Value::basic(offsetof(TileData, name), keg::BasicType::STRING));
    kt.addValue("tex", keg::Value::basic(offsetof(TileData, textureName), keg::BasicType::STRING));
    kt.addValue("col", keg::Value::custom(offsetof(TileData, collisionShape), "TileCollisionShape", true));
    kt.addValue("width", keg::Value::basic(offsetof(TileData, colliderDimsXY.x), keg::BasicType::F32));
    kt.addValue("depth", keg::Value::basic(offsetof(TileData, colliderDimsXY.y), keg::BasicType::F32));
    kt.addValue("height", keg::Value::basic(offsetof(TileData, colliderHeight), keg::BasicType::F32));
    kt.addValue("path_weight", keg::Value::basic(offsetof(TileData, pathWeight), keg::BasicType::UI8));
    kt.addValue("dims", keg::Value::basic(offsetof(TileData, dims), keg::BasicType::UI8_V2));
    kt.addValue("root", keg::Value::basic(offsetof(TileData, rootPos), keg::BasicType::UI8));
    kt.addValue("shape", keg::Value::custom(offsetof(TileData, shape), "TileShape", true));
    kt.addValue("resource", keg::Value::custom(offsetof(TileData, resource), "TileResource", true));
    kt.addValue("drops", keg::Value::array(offsetof(TileData, itemDropsFileData), keg::Value::custom(0, "ItemDropDef", false)));
    kt.addValue("recipe", keg::Value::array(offsetof(TileData, recipeFileData), keg::Value::custom(0, "ItemInputDef", false)));
}