#pragma once

#include "generation/WorldGenerationData.h"

class World;
class Tile;
class Chunk;
class BiomeDef;
struct TileGrass;

class IWorldGenerator
{
public:
    IWorldGenerator(World& world);
    ~IWorldGenerator();

    void generateChunk(Chunk& chunk);

    const WorldGenerationData& getGenerationData() const { return mGenerationData; }
    World& getWorld() const { return mWorld; }
protected:
    virtual Tile generateTileAtPos(const f32v2& worldPos, f32 height, TileGrass* grass, const BiomeDef* biomeDef);

    Tile generateTilePlains(const f32v2& worldPos, f32 height, TileGrass* grass, const BiomeDef* biomeDef);
    Tile generateTileMountains(const f32v2& worldPos, f32 height, TileGrass* grass, const BiomeDef* biomeDef);
    Tile generateTileForests(const f32v2& worldPos, f32 height, TileGrass* grass, const BiomeDef* biomeDef);
    Tile generateTileHotsprings(const f32v2& worldPos, f32 height, TileGrass* grass, const BiomeDef* biomeDef);
   
    void generateTileGrass(const f32v2& worldPos, f32 height, TileGrass* grass);
    
    World& mWorld;
    f32v2 mWorldCenter;
    WorldGenerationData mGenerationData;
};

