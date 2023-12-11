#pragma once

#include "IWorldGenerator.h"

class FlatWorldGenerator : public IWorldGenerator
{
public:
	FlatWorldGenerator(World& world) : IWorldGenerator(world) {};

protected:
	Tile generateTileAtPos(const f32v2& worldPos, f32 height, f32v3 normal, TileGrass* grass, const BiomeDef* biome) override;

};

