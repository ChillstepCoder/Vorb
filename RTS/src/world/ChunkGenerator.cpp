#include "stdafx.h"
#include "ChunkGenerator.h"

#include "Chunk.h"
#include "Noise.h"

void ChunkGenerator::GenerateChunk(Chunk& chunk) {
	const f32v2& chunkPosWorld = chunk.getWorldPos();
	for (int y = 0; y < CHUNK_WIDTH; ++y) {
		for (int x = 0; x < CHUNK_WIDTH; ++x) {
			Tile tile = Tile::TILE_GRASS_0;
			const float n = Noise::fractal(6, 0.6f, 0.01f, x + chunkPosWorld.x, y + chunkPosWorld.y);
			if (n < -0.3) {
				tile = Tile::TILE_STONE_1;
			} else if(n > 0.3) {
				tile = Tile::TILE_GRASS_1;
			}

			chunk.setTileAt(x, y, tile);
		}
	}
	chunk.mState = ChunkState::FINISHED;
}
