#pragma once

#include <random>

#include "util/RollTable.h"

// For rolling random item drops
typedef RollTable<ItemID> ItemRollTable;

// TODO:
YML_WRITE_DEF(ItemRollTable) {
    x;
}