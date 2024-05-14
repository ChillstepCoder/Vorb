#pragma once

#include "world/srv/HostHeightmapGrid.h"
#include "world/biome/BiomeGrid.h"
#include "world/markup/WorldMarkupGrid.h"
#include "world/ownership/OwnershipGrid.h"
#include "world/road/TerrainSurfaceGrid.h"
#include "world/chunk/SimChunkTileGrid.h"

// All word initialization data is here. Can either be generated or loaded
// from disk
class HostWorldData {
public:
    ui32 worldSeed = 0;
    f32v2 playerStart = f32v2(0.5f); // [0, 1]
    ui32 worldWidth = 0;
    // Data layers
    std::shared_ptr<HostHeightmapGrid> heightmapGrid;
    std::shared_ptr<BiomeGrid> biomeGrid;
    std::shared_ptr<WorldMarkupGrid> markupGrid;
    std::shared_ptr<OwnershipGrid> ownershipGrid;
    std::shared_ptr<TerrainSurfaceGrid> terrainSurfaceGrid;
    std::shared_ptr<SimChunkTileGrid> tileGrid;
};