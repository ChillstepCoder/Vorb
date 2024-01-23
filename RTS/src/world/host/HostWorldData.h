#pragma once

#include "world/srv/HostHeightmapGrid.h"
#include "world/biome/BiomeGrid.h"
#include "world/markup/WorldMarkupGrid.h"

// All word initialization data is here. Can either be generated or loaded
// from disk
class HostWorldData {
public:
    ui32 worldSeed = 0;
    f32v2 playerStart = f32v2(0.5f); // [0, 1]
    ui32 worldWidth = 0;
    std::shared_ptr<HostHeightmapGrid> heightmapGrid;
    std::shared_ptr<BiomeGrid> biomeGrid;
    std::shared_ptr<WorldMarkupGrid> markupGrid;
};