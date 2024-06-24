#pragma once

#include "tile/TileHarvestable.h"

struct HarvestableSubchunkRegistry {
    UnorderedFlatMap<TileIndex, TileHarvestable> mHarvestablePositions;
    ui32 mTotalHarvestables[e_cast(TileHarvestable::COUNT)] = {};
};