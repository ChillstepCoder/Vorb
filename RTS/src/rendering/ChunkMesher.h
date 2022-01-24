#pragma once

#include "rendering/TileVertex.h"
#include "world/Tile.h"

#include <Vorb/concurrentqueue.h>

class Chunk;
class TextureAtlas;
class Camera3D;
class WorldGrid;
class QuadMesh;
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
    ChunkMesher(const WorldGrid& worldGrid, const TextureAtlas& textureAtlas);
    ~ChunkMesher();

    void updateMesh(const Chunk& chunk, const f32v3& cameraPos);

private:
    bool createMeshAsync(const Chunk& chunk);

    void addBlock(QuadMesh& quadMesh, f32v3 tilePosition, const HeightmapPatchData* heightData, const TileData& tileData, const TileIndex& tileIndex, const Chunk& chunk, int layerIndex);
    void addBlockVertical(const Chunk& chunk, const TileIndex& tileIndex, int layerIndex, QuadMesh& quadMesh, f32v3 tilePosition, const HeightmapPatchData* heightData, const TileData& tileData);
    void addFloor(QuadMesh& quadMesh, f32v3 tilePosition, const HeightmapPatchData* heightData, const TileData& tileData, const TileIndex& tileIndex, const Chunk& chunk, int layerIndex);
    f32 getTileHeight(const Tile& neighbor, const f32* heightData, TileIndex tileIndex);
    f32 getTileHeight(const TileHandle& neighbor);

    // Shared vertex buffer to eliminate allocations
    const TextureAtlas& mTextureAtlas;
    const WorldGrid& mWorldGrid;
};

