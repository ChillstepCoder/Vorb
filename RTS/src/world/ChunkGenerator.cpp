#include "stdafx.h"
#include "ChunkGenerator.h"

#include "Tile.h"

#include "Chunk.h"
#include "Noise.h"


void ChunkGenerator::GenerateChunk(Chunk& chunk) {

    PreciseTimer timer;

    static TileID grass1 = TileRepository::getTile("grass1");
    static TileID grass2 = TileRepository::getTile("grass2");
    static TileID rock1 = TileRepository::getTile("rock1");

    const f32v2& chunkPosWorld = chunk.getWorldPos();
    for (int y = 0; y < CHUNK_WIDTH; ++y) {
        for (int x = 0; x < CHUNK_WIDTH; ++x) {
            TileID tile = grass1;
            const float n = (float)Noise::fractal(6, 0.6f, 0.01f, (f64)x + (f64)chunkPosWorld.x, (f64)y + (f64)chunkPosWorld.y);
            if (n < -0.3) {
                tile = rock1;
            }
            else if (n > 0.3) {
                tile = grass2;
            }

            chunk.setTileAt(TileIndex(x, y), tile, TileLayer::Ground);
        }
    }
    chunk.mState = ChunkState::FINISHED;

    std::cout << "Chunk generated in " << timer.stop() << " ms\n";
}
