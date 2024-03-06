#pragma once

#include "generation/WorldGenerationData.h"

class TilingVoronoiMap;
class World;
class Tile;
class Chunk;
class SpatialGrid2D;
class SimChunkTileContainer;
class BiomeDef;
struct TileGrass;

class ChunkGenerator
{
public:
    ChunkGenerator(World& world);
    ~ChunkGenerator();

    void generateChunk(Chunk& chunk);
    void generateSimChunk(SimChunkTileContainer& chunk, World& world);

    const WorldGenerationData& getGenerationData() const { return mGenerationData; }
    World& getWorld() const { return mWorld; }
protected:

    Tile generateTileAtPos(i32v2 worldPos, f32 height, f32v3 normal, const BiomeDef* biomeDef);
   
    TileGrass generateTileGrass(i32v2 worldPos, f32 height, f32 intensityMult);
    
    World& mWorld;
    f32v2 mWorldCenter;
    WorldGenerationData mGenerationData;
    std::unique_ptr<SpatialGrid2D> mSpatialGrid;
    
    // Voronoi variant clustering
    std::unique_ptr<TilingVoronoiMap> mVoronoiMap;
};

