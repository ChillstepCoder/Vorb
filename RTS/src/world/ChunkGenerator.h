#pragma once

#include "tile/Tile.h"

class Chunk;
class Region;
class WorldGrid;

struct HeightmapPatchData;

class ChunkGenerator {
public:
	Tile GenerateTileAtPos(const f32v2& worldPos, f32 height, ui8* grass = nullptr);
	void GenerateChunk(Chunk& chunk, WorldGrid& worldGrid, const HeightmapPatchData* heightData);
};

