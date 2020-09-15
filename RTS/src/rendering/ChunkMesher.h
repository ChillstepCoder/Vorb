#pragma once

#include "rendering/BasicVertex.h"

class Chunk;
class TextureAtlas;

constexpr int MAX_VERTICES_PER_CHUNK = CHUNK_SIZE * 4 * 4;
constexpr int MAX_INDICES_PER_CHUNK = CHUNK_SIZE * 4 * 6;

class ChunkMesher {
public:
    ChunkMesher(const TextureAtlas& textureAtlas);

    void createMesh(const Chunk& chunk);

private:
    // Shared vertex buffer to eliminate allocations
    BasicVertex mVertices[MAX_VERTICES_PER_CHUNK];
    const TextureAtlas& mTextureAtlas;
};

