#pragma once

#include "generation/WorldGenerationData.h"

class IWorld;
class Tile;
class Chunk;
struct TileGrass;

class WorldGenerator
{
public:
    WorldGenerator(IWorld& world);
    ~WorldGenerator();

    Tile generateTileAtPos(const f32v2& worldPos, f32 height, TileGrass* grass = nullptr);
    void generateChunk(Chunk& chunk, f32* heightData);
    f32 getTerrainHeightAtPos(const f32v2& worldPos);

    const WorldGenerationData& getGenerationData() const { return mGenerationData; }
private:
    IWorld& mWorld;
    f32v2 mWorldCenter;
    WorldGenerationData mGenerationData;
};

