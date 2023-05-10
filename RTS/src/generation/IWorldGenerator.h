#pragma once

#include "generation/WorldGenerationData.h"

class IWorld;
class Tile;
class Chunk;
struct TileGrass;

class IWorldGenerator
{
public:
    IWorldGenerator(IWorld& world);
    ~IWorldGenerator();

    void generateChunk(Chunk& chunk, f32* heightData);
    virtual f32 getTerrainHeightAtPos(const f32v2& worldPos);

    const WorldGenerationData& getGenerationData() const { return mGenerationData; }
protected:
    virtual Tile generateTileAtPos(const f32v2& worldPos, f32 height, TileGrass* grass = nullptr);

    IWorld& mWorld;
    f32v2 mWorldCenter;
    WorldGenerationData mGenerationData;
};

