#pragma once

#include "Chunk.h"

// A region is a group of chunks, used for LOD in render and simulation, as well as serialization
class Region {
public:
    Chunk* mChunks = nullptr;
};

