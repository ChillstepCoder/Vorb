#include "stdafx.h"
#include "world/Tile.h"

std::unordered_map<std::string, TileID> TileRepository::sTileIdMapping;
std::unordered_map<TileID, TileData> TileRepository::sTileData;

KEG_TYPE_DEF_SAME_NAME(TileData, kt) {
    kt.addValue("name", keg::Value::basic(offsetof(TileData, name), keg::BasicType::STRING));
    kt.addValue("tex", keg::Value::basic(offsetof(TileData, textureName), keg::BasicType::STRING));
    kt.addValue("col", keg::Value::basic(offsetof(TileData, collisionBits), keg::BasicType::UI16));
    kt.addValue("dims", keg::Value::basic(offsetof(TileData, dims), keg::BasicType::UI8_V2));
    kt.addValue("root", keg::Value::basic(offsetof(TileData, rootPos), keg::BasicType::UI8));
}

