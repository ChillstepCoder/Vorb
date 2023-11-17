#pragma once

#include "generation/WorldGenerationData.h"

class World;
class Tile;
class Chunk;
struct TileGrass;

class IWorldGenerator
{
public:
    IWorldGenerator(World& world);
    ~IWorldGenerator();

    void generateChunk(Chunk& chunk);
    virtual f32 getTerrainHeightAtPos(const f32v2& worldPos);

    const WorldGenerationData& getGenerationData() const { return mGenerationData; }
    World& getWorld() const { return mWorld; }
protected:
    virtual Tile generateTileAtPos(const f32v2& worldPos, f32 height, TileGrass* grass = nullptr);

    World& mWorld;
    f32v2 mWorldCenter;
    WorldGenerationData mGenerationData;
};

