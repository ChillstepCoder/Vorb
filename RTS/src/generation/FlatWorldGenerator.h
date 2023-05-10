#pragma once

#include "IWorldGenerator.h"

class FlatWorldGenerator : public IWorldGenerator
{
public:
	FlatWorldGenerator(IWorld& world) : IWorldGenerator(world) {};

	f32 getTerrainHeightAtPos(const f32v2& worldPos) override;

protected:
	Tile generateTileAtPos(const f32v2& worldPos, f32 height, TileGrass* grass = nullptr) override;

};

