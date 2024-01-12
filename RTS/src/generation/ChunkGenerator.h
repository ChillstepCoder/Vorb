#pragma once

#include "generation/WorldGenerationData.h"

class TilingVoronoiMap;
class World;
class Tile;
class Chunk;
class BiomeDef;
struct TileGrass;

class ChunkGenerator
{
public:
    ChunkGenerator(World& world);
    ~ChunkGenerator();

    void generateChunk(Chunk& chunk);

    const WorldGenerationData& getGenerationData() const { return mGenerationData; }
    World& getWorld() const { return mWorld; }
protected:

    Tile generateTileAtPos(f32v2 worldPos, f32 height, f32v3 normal, TileGrass* grass, const BiomeDef* biomeDef);
    Tile generateTileAtPosNew(f32v2 worldPos, f32 height, f32v3 normal, TileGrass* grass, const BiomeDef* biomeDef);

    Tile generateTilePlains(f32v2 worldPos, f32 height, TileGrass* grass, const BiomeDef* biomeDef);
    Tile generateTileMountains(f32v2 worldPos, f32 height, TileGrass* grass, const BiomeDef* biomeDef);
    Tile generateTileForests(f32v2 worldPos, f32 height, TileGrass* grass, const BiomeDef* biomeDef);
    Tile generateTileHotsprings(f32v2 worldPos, f32 height, f32v3 normal, TileGrass* grass, const BiomeDef* biomeDef);
   
    void generateTileGrass(f32v2 worldPos, f32 height, TileGrass* grass);
    
    World& mWorld;
    f32v2 mWorldCenter;
    WorldGenerationData mGenerationData;
    
    // Voronoi variant clustering
    std::unique_ptr<TilingVoronoiMap> mVoronoiMap;
};

