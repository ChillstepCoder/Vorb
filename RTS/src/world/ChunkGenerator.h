#pragma once

#include "Tile.h"

class Chunk;
class Region;

class ChunkGenerator {
public:
	Tile GenerateTileAtPos(const f32v2& worldPos);
	void GenerateChunk(Chunk& chunk);
	void GenerateRegionLODTextureAsync(Region& region, color3* recursivePixelBuffer = nullptr);
};

