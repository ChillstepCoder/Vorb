#pragma once

#include "world/IChunkGrid.h"

class SrvChunkGrid : public IChunkGrid
{
public:
    SrvChunkGrid(ui32 widthChunks) : IChunkGrid(widthChunks) {};
};

