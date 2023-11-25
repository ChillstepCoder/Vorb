#pragma once

#include "world/srv/HostHeightmapGrid.h"
#include "world/biome/BiomeGrid.h"

// All word initialization data is here. Can either be generated or loaded
// from disk
class HostWorldData {
public:
    f32v2 playerStart = f32v2(0.5f); // [0, 1]
    ui32 worldWidth = 0;
    std::unique_ptr<HostHeightmapGrid> heightmapGrid;
    std::unique_ptr<BiomeGrid> biomeGrid;
};