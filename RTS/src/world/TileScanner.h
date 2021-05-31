#pragma once

#include "world/Chunk.h"

class World;

// Scan for and collect specific tiles or tile types
class TileScanner
{
public:
    // Returns vector of found tiles in order from closest to furthest
    static std::vector<LiteTileHandle> scanForResource(World& world, TileResource resource, const ui32v2& startWorldPos, ui32 maxDistance, ui32 maxTilesToReturn = UINT32_MAX);

private:
};

