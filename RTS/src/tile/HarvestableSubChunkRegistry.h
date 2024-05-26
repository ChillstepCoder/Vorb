#pragma once

#include "tile/TileHarvestable.h"

struct HarvestableSubchunkRegistry {
    boost::container::flat_map<TileIndex, TileHarvestable> mHarvestablePositions;
    ui32 mTotalHarvestables[e_cast(TileHarvestable::COUNT)] = {};
};