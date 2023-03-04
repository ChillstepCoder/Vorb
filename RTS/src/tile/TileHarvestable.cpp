#include "stdafx.h"
#include "TileHarvestable.h"


KEG_ENUM_DEF(TileHarvestable, TileHarvestable, kt) {
    kt.addValue("none", TileHarvestable::NONE);
    kt.addValue("wood", TileHarvestable::WOOD);
    kt.addValue("stone", TileHarvestable::STONE);
}