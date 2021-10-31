#include "stdafx.h"
#include "Building.h"


KEG_ENUM_DEF(BuildingFunction, BuildingFunction, kt) {
    kt.addValue("none", BuildingFunction::NONE);
    kt.addValue("residence", BuildingFunction::RESIDENCE);
    kt.addValue("lumbermill", BuildingFunction::LUMBERMILL);
}
static_assert(enum_cast(BuildingFunction::TYPES) == 3, "Update keg definition");
