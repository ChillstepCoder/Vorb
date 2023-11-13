#pragma once

#include "world/simulation/SimChunk.h"

class ChunkSimulator
{
    // Only HostSimContext may use this class
    friend class HostSimContext;
protected:
    // Returns new sim chunk state
    // Sim chunk must be in SimChunkState::Simulating to be used in this function
    SimChunkState simulateChunk(SimChunkData& data, ChunkID chunkId);
};

