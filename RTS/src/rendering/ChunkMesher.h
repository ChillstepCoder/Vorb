#pragma once

#include "rendering/TileVertex.h"

class Chunk;
class TextureAtlas;

constexpr int MAX_VERTICES_PER_CHUNK = CHUNK_SIZE * 4 * 4;
constexpr int MAX_INDICES_PER_CHUNK = CHUNK_SIZE * 4 * 6;

// TODO: Move to Light.h?
struct StaticLight {
    f32v3 position;
    color4 color;
    f32 radius;
    //vector<f32> attenuationCurve;
};

class ChunkMesher {
public:
    ChunkMesher(const TextureAtlas& textureAtlas);

    // Updatemesh?
    // CreateMesh also caches shadow casters so we dont
    // iterate through full data when updating shadows
    void createMesh(const Chunk& chunk);

private:
    // Shared vertex buffer to eliminate allocations
    TileVertex mVertices[MAX_VERTICES_PER_CHUNK];
    const TextureAtlas& mTextureAtlas;
};

