#pragma once

#include "rendering/ChunkVertex.h"

class Chunk;

constexpr int MAX_VERTICES_PER_CHUNK = CHUNK_SIZE * 4 * 4;
constexpr int MAX_INDICES_PER_CHUNK = CHUNK_SIZE * 4 * 6;

class ChunkMesher {
public:
    ChunkMesher();

    void createMesh(const Chunk& chunk);
    void updateSpritebatch(const Chunk& chunk);

private:
    // Shared vertex buffer to eliminate allocations
    ChunkVertex mVertices[MAX_VERTICES_PER_CHUNK];
    ui32 mIndices[MAX_INDICES_PER_CHUNK];
};

