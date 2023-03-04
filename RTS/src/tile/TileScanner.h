#pragma once

#include "world/Chunk.h"

// Scan for and collect specific tiles or tile types
class TileScanner
{
public:
    // Returns vector of found tiles in order from closest to furthest
    static std::vector<TileHandle> scanForHarvestable(TileHarvestable resource, const ui32v2& startWorldPos, ui32 maxDistance, ui32 maxTilesToReturn = UINT32_MAX);

private:
};

