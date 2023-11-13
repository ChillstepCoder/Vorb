#pragma once

#include "world/srv/HostHeightmapGrid.h"

class HostWorldData {
public:
    std::unique_ptr<HostHeightmapGrid> heightmapGrid;
};