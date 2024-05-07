#pragma once
#include "world/IChunkGrid.h"

class CliChunkGrid : public IChunkGrid
{
public:
    CliChunkGrid() : IChunkGrid() {};
private:
    void updateActivatingChunks() override;
};

