#pragma once

#include "world/Chunk.h"

class World;

// Scan for and collect specific tiles or tile types
class TileScanner
{
public:
    std::vector<LiteTileHandle> scanForTiles(World& world, const TileID tileIDs[], ui32 numTileIDs, const ui32v2& startWorldPos, ui32 maxDistance);

private:
};

