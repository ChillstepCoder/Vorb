#pragma once

#include "rendering/TileVertex.h"
#include "world/ChunkID.h" // For tile position
#include "world/Tile.h"

#include <Vorb/concurrentqueue.h>

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

    void addBlock(MeshBuilder& quadMeshBuilder, f32v3 tilePosition, const TileData& tileData, const TileIndex& tileIndex, const Chunk& chunk);
    void addBlockVertical(const Chunk& chunk, const TileIndex& tileIndex, MeshBuilder& quadMeshBuilder, f32v3 tilePosition, const TileData& tileData);
    void addFloor(MeshBuilder& quadMeshBuilder, f32v3 tilePosition, const HeightmapPatchData* heightData, const TileData& tileData, const TileIndex& tileIndex, const Chunk& chunk);
    f32 getTileHeight(const Tile& neighbor, const f32* heightData, TilePosition tilePos);
    f32 getTileHeight(const TileHandle& neighbor);

    // Shared vertex buffer to eliminate allocations
    const WorldGrid& mWorldGrid;
};

