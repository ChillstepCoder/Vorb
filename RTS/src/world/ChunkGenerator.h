#pragma once

#include "tile/Tile.h"

class Chunk;
struct TileGrass;

struct HeightmapPatchData;

// TODO: Check out this glsl impl: https://github.com/GabeRundlett/voxel_game_public/blob/master/shaders/utils/noise.glsl
class ChunkGenerator {
public:
	static Tile GenerateTileAtPos(const f32v2& worldPos, f32 height, TileGrass* grass = nullptr);
	static void GenerateChunk(Chunk& chunk, f32* heightData);
};

