#pragma once

#include "util/RollTable.h"

// For rolling random item drops

using ItemRollTable = RollTable<ItemAssetRef>;

// TODO:
YML_WRITE_DEF(ItemRollTable) {
    o.ymlWrite(*n);
}

YML_READ_DEF(ItemRollTable) {
    return target->ymlRead(n);
}