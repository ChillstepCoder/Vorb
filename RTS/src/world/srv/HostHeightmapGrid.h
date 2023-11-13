#pragma once

#include "world/IHeightmapGrid.h"

class HostHeightmapGrid : public IHeightmapGrid
{
public:
    HostHeightmapGrid(ui32 worldWidthTiles) : IHeightmapGrid(worldWidthTiles) {}
};

