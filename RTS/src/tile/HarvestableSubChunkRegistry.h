#pragma once

#include "tile/TileHarvestable.h"
#include "boost/container/flat_map.hpp"

struct HarvestableSubchunkRegistry {
    boost::container::flat_map<TileIndex, TileHarvestable> mHarvestablePositions;
    ui32 mTotalHarvestables[e_cast(TileHarvestable::COUNT)] = {};
};