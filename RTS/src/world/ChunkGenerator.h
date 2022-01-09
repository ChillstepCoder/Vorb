#pragma once

#include "Tile.h"

class Chunk;
class Region;
class WorldGrid;

class ChunkGenerator {
public:
	Tile GenerateTileAtPos(const f32v2& worldPos, f32 height, ui8* grass = nullptr);
	void GenerateChunk(Chunk& chunk, WorldGrid& worldGrid, const f32* heightData);
};

