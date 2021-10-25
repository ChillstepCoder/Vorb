#include "stdafx.h"
#include "world/Tile.h"

#include "world/TileRepository.h"


KEG_ENUM_DEF(TileShape, TileShape, kt) {
    kt.addValue("Thin", TileShape::THIN);
    kt.addValue("Thick", TileShape::BLOCK);
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

KEG_TYPE_DEF_SAME_NAME(ItemDropDef, kt) {
    kt.addValue("item", keg::Value::basic(offsetof(ItemDropDef, itemName), keg::BasicType::STRING));
    kt.addValue("count", keg::Value::basic(offsetof(ItemDropDef, countRange), keg::BasicType::UI32_V2));
}


TileCollision Tile::buildTileCollision() const {
    TileCollision collision;
    collision.baseZPosition = baseZPosition;

    // Custom tile collision only occurs on TILE_LAYER_TOP
    const TileData& tileData = TileRepository::getTileData(topLayer);
    collision.shape = tileData.collisionShape;
    collision.colliderHeightUnscaled = (ui16)(tileData.colliderHeight * UINT8_MAX);
    collision.colliderDimsUnscaledXY = i8v2(glm::round(tileData.colliderDimsXY * (f32)TILE_COLLIDER_DIMS_SCALE));
    collision.pathWeight = tileData.pathWeight;

    return collision;
}
