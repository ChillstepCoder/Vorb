#pragma once

#include "tile/Tile.h"

class Chunk;
class Region;

struct HeightmapPatchData;

class ChunkGenerator {
public:
	static Tile GenerateTileAtPos(const f32v2& worldPos, f32 height, ui8* grass = nullptr);
	static void GenerateChunk(Chunk& chunk, const HeightmapPatchData* heightData);
};

