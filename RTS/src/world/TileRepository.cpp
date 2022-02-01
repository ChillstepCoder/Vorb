#include "stdafx.h"
#include "TileRepository.h"

std::unordered_map<std::string, TileID> TileRepository::sTileIdMapping;
std::vector<TileData> TileRepository::sTileData;

KEG_TYPE_DEF_SAME_NAME(TileFileData, kt) {
    kt.addValue("tex", keg::Value::basic(offsetof(TileFileData, textureName), keg::BasicType::STRING));
    kt.addValue("col", keg::Value::custom(offsetof(TileFileData, colliderShape), "TileCollisionShape", true));
    kt.addValue("width", keg::Value::basic(offsetof(TileFileData, colliderDims.x), keg::BasicType::F32));
    kt.addValue("depth", keg::Value::basic(offsetof(TileFileData, colliderDims.y), keg::BasicType::F32));
    kt.addValue("height", keg::Value::basic(offsetof(TileFileData, colliderDims.z), keg::BasicType::F32));
    kt.addValue("path_weight", keg::Value::basic(offsetof(TileFileData, pathWeight), keg::BasicType::UI8));
    kt.addValue("layer", keg::Value::basic(offsetof(TileFileData, layer), keg::BasicType::UI8));
    kt.addValue("col_dims", keg::Value::basic(offsetof(TileFileData, colliderDims.x), keg::BasicType::F32_V3));
    kt.addValue("shape", keg::Value::custom(offsetof(TileFileData, tileShape), "TileShape", true));
    kt.addValue("resource", keg::Value::custom(offsetof(TileFileData, resource), "TileResource", true));
    kt.addValue("drops", keg::Value::array(offsetof(TileFileData, itemDrops), keg::Value::custom(0, "ItemDropDef", false)));
    kt.addValue("recipe", keg::Value::array(offsetof(TileFileData, recipe), keg::Value::custom(0, "ItemInputDef", false)));
}