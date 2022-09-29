#pragma once

#include "tile/Tile.h"

class Chunk;
class Camera3D;
class IWorldGrid;
class MeshBuilder;
class PhysicsWorld;
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

    // TODO: Actually update the physics mesh
    static void updateMeshAndPhysics(const Chunk& chunk, const f32v3& cameraPos);

private:
    static bool createMeshAndPhysicsAsync(const Chunk& chunk);
};

