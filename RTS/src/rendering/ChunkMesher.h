#pragma once

#include "rendering/TileVertex.h"

#include <Vorb/concurrentqueue.h>

class Chunk;
class TextureAtlas;
class Camera3D;
struct TileData;
struct SpriteData;


struct TileMeshData {
    // TODO: Delete? no longer need
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

    void updateMesh(const Chunk& chunk, const f32v3& cameraPos);

private:
    bool createMeshAsync(const Chunk& chunk);
    bool createHighDetailFloraMeshAsync(const Chunk& chunk);

    TileMeshData* tryGetFreeTileMeshData();

    // Shared vertex buffer to eliminate allocations
    const TextureAtlas& mTextureAtlas;
    // Passed to worker threads for use, then returned to storage
    std::vector<TileMeshData*> mFreeTileMeshData;
    int mNumMeshTasksRunning = 0;
};

