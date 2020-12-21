#pragma once

#include "rendering/TileVertex.h"

#include <Vorb/concurrentqueue.h>

class Chunk;
class TextureAtlas;

constexpr int MAX_VERTICES_PER_CHUNK = CHUNK_SIZE * 4 * 4;
constexpr int MAX_INDICES_PER_CHUNK = CHUNK_SIZE * 4 * 6;
constexpr int AVERAGE_VERTICES_PER_CHUNK = MAX_VERTICES_PER_CHUNK / 2;

struct TileMeshData {
    TileMeshData() {
        mTileVertices.reserve(AVERAGE_VERTICES_PER_CHUNK);
    }
    std::vector<TileVertex> mTileVertices;
};

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
    ~ChunkMesher();

    // Updatemesh?
    void createFullDetailMeshAsync(const Chunk& chunk);
    void createFullDetailMesh(const Chunk& chunk);
    void createLODTexture(const Chunk& chunk);

private:
    // Shared vertex buffer to eliminate allocations
    const TextureAtlas& mTextureAtlas;
    // Passed to worker threads for use, then returned to storage
    std::vector<TileMeshData*> mFreeTileMeshData;
    int mNumMeshTasksRunning = 0;

    color3 mLODTexturePixelBuffer[CHUNK_SIZE];
};

