#pragma once

#include "world/Tile.h"

class Chunk;
class Camera3D;
class WorldGrid;
class MeshBuilder;
struct TileData;
class Tile;
struct SpriteData;
struct HeightmapPatchData;
struct TileHandle;

// TODO: Move to Light.h?
struct StaticLight {
    f32v3 position;
    color4 color;
    f32 radius;
    //vector<f32> attenuationCurve;
};

class ChunkMesher {
public:
    ChunkMesher(const WorldGrid& worldGrid);
    ~ChunkMesher();

    void updateMesh(const Chunk& chunk, const f32v3& cameraPos);

private:
    bool createMeshAsync(const Chunk& chunk);

    // Shared vertex buffer to eliminate allocations
    const WorldGrid& mWorldGrid;
};

