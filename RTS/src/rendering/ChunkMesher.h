#pragma once

#include "rendering/TileVertex.h"

#include <Vorb/concurrentqueue.h>

class Chunk;
class TextureAtlas;
struct TileData;
struct SpriteData;


struct TileMeshData {
    color3 mLODTexturePixelBuffer[CHUNK_SIZE];
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

    void set3DMeshMode(bool should3DMesh) { m3DMeshMode = should3DMesh; }

    // Updatemesh?
    bool createMeshAsync(const Chunk& chunk);
    bool createLODTextureAsync(const Chunk& chunk);
    bool createHighDetailFloraMeshAsync(const Chunk& chunk);

private:

    TileMeshData* tryGetFreeTileMeshData();

    // Shared vertex buffer to eliminate allocations
    const TextureAtlas& mTextureAtlas;
    // Passed to worker threads for use, then returned to storage
    std::vector<TileMeshData*> mFreeTileMeshData;
    int mNumMeshTasksRunning = 0;

    bool m3DMeshMode = false;
};

