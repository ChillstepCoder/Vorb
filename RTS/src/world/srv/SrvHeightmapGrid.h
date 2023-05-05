#pragma once

#include "world/IHeightmapGrid.h"

class SrvHeightmapGrid : public IHeightmapGrid
{
public:
    SrvHeightmapGrid(ui32 worldWidthTiles) : IHeightmapGrid(worldWidthTiles) {}
};

