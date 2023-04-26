#pragma once
#include "world/IChunkGrid.h"

class CliChunkGrid : public IChunkGrid
{
public:
    CliChunkGrid(ui32 widthChunks) : IChunkGrid(widthChunks) {};
private:

};

