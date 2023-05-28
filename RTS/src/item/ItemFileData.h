#pragma once

#include "item/ItemDef.h"

struct ItemFileData {
    nString name;
    nString textureName;
    ItemType type = ItemType::UNKNOWN;
    ItemStorageShape shape = ItemStorageShape::POINT;
    f32 value = 1.0f;
    f32 weight = 0.01f;
    ui32 stackSize = 10;
    ui32v3 stackDims = ui32v3(5, 5, 5);
    TileHarvestable harvestableSource = TileHarvestable::NONE;
};
KEG_TYPE_DECL(ItemFileData);