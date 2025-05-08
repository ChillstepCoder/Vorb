#pragma once
#include "world/IHeightmapGrid.h"

class CliHeightmapGrid : public IHeightmapGrid
{
public:
    CliHeightmapGrid(ui32 worldWidthTiles) : IHeightmapGrid(worldWidthTiles) {}
};

