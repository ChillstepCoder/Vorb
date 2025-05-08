#pragma once

#include "generation/WorldGenerationData.h"

class TilingVoronoiMap;
class World;
class Tile;
class LocalChunk;
class SpatialGrid2D;
class SimChunk;
class BiomeDef;
class BitArray;
struct TileGrass;

class ChunkGenerator
{
public:
    ChunkGenerator(World& world);
    ~ChunkGenerator();

    // Generate a chunk from the sim chunk, and rectify any invalid data such
    // as trees intersecting buildings, removing them from the simchunk
    void generateChunkFromSimChunk(LocalChunk& chunk, const BitArray& buildingFootprint);
    // Generate a simulated chunk, may contain some invalid data such as trees intersecting
    // buildings, but such errors are fixed during full chunk generation
    void generateSimChunk(SimChunk& chunk, World& world);

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

