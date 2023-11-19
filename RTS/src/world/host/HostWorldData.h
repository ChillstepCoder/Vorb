#pragma once

#include "world/srv/HostHeightmapGrid.h"

// All word initialization data is here. Can either be generated or loaded
// from disk
class HostWorldData {
public:
    ui32 worldWidth = 0;
    std::unique_ptr<HostHeightmapGrid> heightmapGrid;
};