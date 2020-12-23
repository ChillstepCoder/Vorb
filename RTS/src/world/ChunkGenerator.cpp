#include "stdafx.h"
#include "ChunkGenerator.h"

#include "Tile.h"

#include "Chunk.h"
#include "Noise.h"
#include "Random.h"

void ChunkGenerator::GenerateChunk(Chunk& chunk) {

    // TODO: Generate a continent that is roughly 8 miles by 8 miles

    // ThreadSafe
    constexpr int OCTAVES = 7;
    constexpr float PERSISTENCE = 0.7f;
    constexpr float FREQUENCY = 0.001f;
    constexpr f64 START_OFFSET = 1000.0;

    PreciseTimer timer;

    static TileID grass1 = TileRepository::getTile("grass1");
    static TileID grass2 = TileRepository::getTile("grass2");
    static TileID rock1 = TileRepository::getTile("rock1");
    static TileID bigTree = TileRepository::getTile("tree_large");
    static TileID smallTree = TileRepository::getTile("tree_small");
    static TileID water = TileRepository::getTile("water");

    const f32v2& chunkPosWorld = chunk.getWorldPos();
    for (int y = 0; y < CHUNK_WIDTH; ++y) {
        for (int x = 0; x < CHUNK_WIDTH; ++x) {
            TileID tile = grass1;
            TileIndex index(x, y);
            const f64 height = -Noise::fractal(OCTAVES, PERSISTENCE, FREQUENCY, (f64)x + (f64)chunkPosWorld.x + START_OFFSET, (f64)y + (f64)chunkPosWorld.y + START_OFFSET);
            
            const f32v2 offsetToCenter = chunkPosWorld + f32v2(CHUNK_WIDTH / 2);
            constexpr f32 CONTINENT_RADIUS = 100.0f;

            
            if (height > 0.3) {
                tile = rock1;
            }
            else if (height < -0.25) {
                tile = water;
            }
            else if (height < -0.1 || height > 0.1) {
                if (Random::getThreadSafef(x, y) > 0.95f) {
                    chunk.setTileFromGeneration(index, smallTree, TileLayer::Mid);
                }
            }
            else {
                tile = grass2;
                if (Random::getThreadSafef(x, y) > 0.6f) {
                    chunk.setTileFromGeneration(index, bigTree, TileLayer::Mid);
                }
            }

            chunk.setTileFromGeneration(index, tile, TileLayer::Ground);
        }
    }

    std::cout << "Chunk generated in " << timer.stop() << " ms\n";
}
