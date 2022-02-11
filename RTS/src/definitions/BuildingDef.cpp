#include "stdafx.h"
#include "BuildingDef.h"

KEG_ENUM_DEF(BuildingFunction, BuildingFunction, kt) {
    kt.addValue("none", BuildingFunction::NONE);
    kt.addValue("residence", BuildingFunction::RESIDENCE);
    kt.addValue("lumbermill", BuildingFunction::LUMBERMILL);
}
static_assert(enum_cast(BuildingFunction::TYPES) == 3, "Update keg definition");

KEG_ENUM_DEF(RoomType, RoomType, kt) {
    kt.addValue("none", RoomType::NONE);
    kt.addValue("stockpile", RoomType::STOCKPILE);
    kt.addValue("shop", RoomType::SHOP);
    kt.addValue("workshop", RoomType::WORKSHOP);
    kt.addValue("lavatory", RoomType::LAVATORY);
    kt.addValue("kitchen", RoomType::KITCHEN);
    kt.addValue("dining_room", RoomType::DINING_ROOM);
    kt.addValue("bedroom", RoomType::BEDROOM);
}
static_assert(enum_cast(RoomType::TYPES) == 8, "Update keg definition");

KEG_TYPE_DEF_SAME_NAME(RoomDef, kt) {
    kt.addValue("type", keg::Value::custom(offsetof(RoomDef, roomType), "RoomType", true));
    kt.addValue("min_width", keg::Value::basic(offsetof(RoomDef, minWidth), keg::BasicType::UI8));
    kt.addValue("max_width", keg::Value::basic(offsetof(RoomDef, maxWidth), keg::BasicType::UI8));
}
