#pragma once

#include "world/Region.h"

class WorldGrid {
public:
    WorldGrid();

    Chunk& getChunk(ui32 i) { return mChunks[i]; }
    const Chunk& getChunk(ui32 i) const { return mChunks[i]; }
    
    Region& getRegion(ui32 i) { return mRegions[i]; }
    const Region& getRegion(ui32 i) const { return mRegions[i]; }

    static ui32 numChunks() { return WorldData::WORLD_SIZE_CHUNKS; }
    static ui32 numRegions() { return WorldData::WORLD_SIZE_REGIONS; }

private:
    Chunk mChunks[WorldData::WORLD_SIZE_CHUNKS];
    Region mRegions[WorldData::WORLD_SIZE_REGIONS];
};