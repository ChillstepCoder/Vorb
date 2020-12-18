#include "stdafx.h"
#include "ChunkGenerator.h"

#include "Tile.h"

#include "Chunk.h"
#include "Noise.h"
#include "Random.h"

void ChunkGenerator::GenerateChunk(Chunk& chunk) {

    // ThreadSafe

    PreciseTimer timer;

    static TileID grass1 = TileRepository::getTile("grass1");
    static TileID grass2 = TileRepository::getTile("grass2");
    static TileID rock1 = TileRepository::getTile("rock1");
    static TileID bigTree = TileRepository::getTile("tree_large");
    static TileID smallTree = TileRepository::getTile("tree_small");

    const f32v2& chunkPosWorld = chunk.getWorldPos();
    for (int y = 0; y < CHUNK_WIDTH; ++y) {
        for (int x = 0; x < CHUNK_WIDTH; ++x) {
            TileID tile = grass1;
            TileIndex index(x, y);
            const float n = (float)Noise::fractal(6, 0.6f, 0.01f, (f64)x + (f64)chunkPosWorld.x, (f64)y + (f64)chunkPosWorld.y);
            if (n < -0.3) {
                tile = rock1;
            }
            else if (n > 0.3) {
                tile = grass2;
                if (Random::getThreadSafef(x, y) > 0.6f) {
                    chunk.setTileFromGeneration(index, bigTree, TileLayer::Mid);
                }
            }
            else {
                if (Random::getThreadSafef(x, y) > 0.95f) {
                    chunk.setTileFromGeneration(index, smallTree, TileLayer::Mid);
                }
            }

            chunk.setTileFromGeneration(index, tile, TileLayer::Ground);
        }
    }

    std::cout << "Chunk generated in " << timer.stop() << " ms\n";
}
