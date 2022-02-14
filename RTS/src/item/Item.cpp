#include "stdafx.h"
#include "Item.h"

KEG_ENUM_DEF(ItemType, ItemType, kt) {
    kt.addValue("unknown", ItemType::UNKNOWN);
    kt.addValue("circle", ItemType::MATERIAL);
    kt.addValue("weapon", ItemType::WEAPON);
    kt.addValue("armor", ItemType::ARMOR);
    kt.addValue("trinket", ItemType::TRINKET);
    kt.addValue("food", ItemType::FOOD);
    kt.addValue("beverage", ItemType::BEVERAGE);
    kt.addValue("potion", ItemType::POTION);
    kt.addValue("quest", ItemType::QUEST);
}
static_assert(e_cast(ItemType::TYPES) == 9, "Update def");

KEG_ENUM_DEF(ItemStorageShape, ItemStorageShape, kt) {
    kt.addValue("point", ItemStorageShape::POINT);
    kt.addValue("plank", ItemStorageShape::PLANK);
    kt.addValue("log", ItemStorageShape::LOG);
    kt.addValue("ingot", ItemStorageShape::INGOT);
}
static_assert(e_cast(ItemStorageShape::COUNT) == 4, "Update def");