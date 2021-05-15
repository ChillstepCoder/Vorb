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
static_assert(enum_cast(ItemType::TYPES) == 9, "Update def");