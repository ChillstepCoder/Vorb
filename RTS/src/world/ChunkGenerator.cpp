#include "stdafx.h"
#include "ChunkGenerator.h"

#include "Chunk.h"
#include "Noise.h"
#include "Random.h"

#include "world/WorldData.h"
#include "world/Region.h"

#include "services/Services.h"

// Terrain gen constants
constexpr int OCTAVES = 7;
constexpr float PERSISTENCE = 0.7f;
constexpr float FREQUENCY = 0.001f;
constexpr f64 CONTINENT_RADIUS = 12000.0;
constexpr f64 CONTINENT_RADIUS_SQ = SQ(CONTINENT_RADIUS);

// Region LOD data
#ifdef DEBUG
constexpr int LOD_TEXTURE_RESOLUTION = CHUNK_WIDTH;
#else
constexpr int LOD_TEXTURE_RESOLUTION = CHUNK_WIDTH * 4;
#endif
constexpr float LOD_STRIDE = WorldData::REGION_WIDTH_TILES / LOD_TEXTURE_RESOLUTION;

Tile ChunkGenerator::GenerateTileAtPos(const f32v2& worldPos) {

    // TODO: This seems wrong
    static TileID grass1 = TileRepository::getTile("grass1");
    static TileID grass2 = TileRepository::getTile("grass2");
    static TileID rock1 = TileRepository::getTile("rock1");
    static TileID bigTree = TileRepository::getTile("tree_large");
    static TileID smallTree = TileRepository::getTile("tree_small");
    static TileID water = TileRepository::getTile("water");

    Tile tile(grass1, TILE_ID_NONE, TILE_ID_NONE);

    f64 height = -Noise::fractal(OCTAVES, PERSISTENCE, FREQUENCY, (f64)worldPos.x, (f64)worldPos.y);

    f32v2 offsetToCenter(
        worldPos.x - WorldData::WORLD_CENTER.x,
        worldPos.y - WorldData::WORLD_CENTER.y
    );

    const f64 distanceFromCenter2 = glm::length2(offsetToCenter);
    if (distanceFromCenter2 > CONTINENT_RADIUS_SQ) {
        height -= (distanceFromCenter2 - CONTINENT_RADIUS_SQ) * 0.0000001;
    }

    if (height > 0.3) {
        tile.groundLayer = rock1;
    }
    else if (height < -0.45) {
        tile.groundLayer = water;
    }
    else if (height < -0.1 || height > 0.1) {
        if (Random::getThreadSafef(offsetToCenter.x, offsetToCenter.y) > 0.95f) {
            tile.midLayer = smallTree;
        }
    }
    else {
        tile.groundLayer = grass2;
        if (Random::getThreadSafef(offsetToCenter.x, offsetToCenter.y) > 0.6f) {
            tile.midLayer = bigTree;
        }
    }
    return tile;
}

void ChunkGenerator::GenerateChunk(Chunk& chunk) {

    PreciseTimer timer;

    // Allocate tiles if needed
    if (!chunk.mTiles.size()) {
        chunk.allocateTiles();
    }

    const f32v2& chunkPosWorld = chunk.getWorldPos();
    for (int y = 0; y < CHUNK_WIDTH; ++y) {
        for (int x = 0; x < CHUNK_WIDTH; ++x) {
            const f32v2 tilePosWorld(x + chunkPosWorld.x, y + chunkPosWorld.y);
            chunk.setTileFromGeneration(TileIndex(x, y), GenerateTileAtPos(tilePosWorld));
        }
    }

    std::cout << "Chunk generated in " << timer.stop() << " ms\n";
}

void ChunkGenerator::GenerateRegionLODTextureAsync(Region& region)
{

    color3* pixelData = new color3[LOD_TEXTURE_RESOLUTION * LOD_TEXTURE_RESOLUTION];

    Services::Threadpool::ref().addTask([&, pixelData](ThreadPoolWorkerData* workerData) {
        const f32v2& regionPosWorld = region.getWorldPos();
        color3* currentPixel = pixelData;
        for (int y = 0; y < LOD_TEXTURE_RESOLUTION; ++y) {
            for (int x = 0; x < LOD_TEXTURE_RESOLUTION; ++x) {
                const f32v2 tilePosWorld(x * LOD_STRIDE + regionPosWorld.x, y * LOD_STRIDE + regionPosWorld.y);
                Tile tile = GenerateTileAtPos(tilePosWorld);
                for (int l = TILE_LAYER_COUNT - 1; l >= 0; --l) {
                    TileID layerTile = tile.layers[l];
                    if (layerTile == TILE_ID_NONE) {
                        continue;
                    }
                    // First opaque tile
                    const TileData& tileData = TileRepository::getTileData(layerTile);
                    *currentPixel++ = tileData.spriteData.lodColor;
                    break;
                }
            }
        }
    }, [&, pixelData]() {
        RegionRenderData& renderData = region.mRenderData;
        if (!renderData.mLODTexture) {
            glGenTextures(1, &renderData.mLODTexture);
        }

        glBindTexture(GL_TEXTURE_2D, renderData.mLODTexture);
        // Compressed
        glTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGB, LOD_TEXTURE_RESOLUTION, LOD_TEXTURE_RESOLUTION, 0, GL_RGB, GL_UNSIGNED_BYTE, pixelData);

        // Set up tex parameters
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

        glBindTexture(GL_TEXTURE_2D, 0);

        region.mRenderData.mLODDirty = false;

        delete[] pixelData;
    });
}
