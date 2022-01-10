#include "stdafx.h"
#include "ChunkGenerator.h"

#include "Chunk.h"
#include "Noise.h"
#include "Random.h"

#include "world/WorldData.h"
#include "world/WorldGrid.h"
#include "world/Region.h"
#include "world/TileRepository.h"

#include "services/Services.h"

#include "generation/NoiseFunction.hpp"
#include "generation/WorldGeneration.h"

// Region LOD data
#ifdef DEBUG
constexpr int LOD_TEXTURE_RESOLUTION = CHUNK_WIDTH;
#else
constexpr int LOD_TEXTURE_RESOLUTION = CHUNK_WIDTH * 4;
#endif
constexpr float LOD_STRIDE = WorldData::REGION_WIDTH_TILES / LOD_TEXTURE_RESOLUTION;

Tile ChunkGenerator::GenerateTileAtPos(const f32v2& worldPos, f32 height, ui8* grass) {

    // TODO: This seems wrong
    static TileID grass1 = TileRepository::getTile("grass1");
    static TileID grass2 = TileRepository::getTile("grass2");
    static TileID rock1 = TileRepository::getTile("rock1");
    static TileID hugeTree = TileRepository::getTile("tree_huge");
    static TileID bigTree = TileRepository::getTile("tree_large");
    static TileID smallTree = TileRepository::getTile("tree_small");
    static TileID water = TileRepository::getTile("water");

    static TileID flowers = TileRepository::getTile("flowers");
    static TileID flower = TileRepository::getTile("flower");
    static TileID shrub = TileRepository::getTile("shrub");
    static TileID smallBush = TileRepository::getTile("small_bush");
    static TileID bush = TileRepository::getTile("bush");
    static TileID largeBush = TileRepository::getTile("large_bush");

    Tile tile(TILE_ID_NONE, TILE_ID_NONE, TILE_ID_NONE);
    f32v2 offsetToCenter(
        worldPos.x - WorldData::WORLD_CENTER.x,
        worldPos.y - WorldData::WORLD_CENTER.y
    );

    tile.baseZPosition = height;

    if (grass && Random::getThreadSafef(offsetToCenter.x, worldPos.y) > 0.02f) {
        *grass = 1;
    }

    //if (height > 0.3) {
    //    //tile.groundLayer = rock1;
    //    // Mountains
    //    tile.baseZPosition = (ui16)((height - 0.3) / 0.004) + 1u;
    //}
    //else if (height < -0.45) {
    //    //tile.groundLayer = water;
    //}
    //else if (height < -0.1 || height > 0.1) {
    //    // Standard grass layer
    //    if (abs(height) < 0.3) {
    //        // Fields
    //        const float fNoise = sWorldGen.mFlowerNoise.compute((f64)worldPos.x, (f64)worldPos.y);
    //        if (Random::getThreadSafef(offsetToCenter.x, worldPos.y) > 0.999f) {
    //            tile.topLayer = bush;
    //        }
    //        else if (Random::getThreadSafef(worldPos.y, worldPos.x) > 0.999f) {
    //            tile.topLayer = smallBush;
    //        }
    //        else if (Random::getThreadSafef(worldPos.x, offsetToCenter.y) > 0.99f) {
    //            tile.topLayer = shrub;
    //        }
    //        else if (fNoise > 0.35f && Random::getThreadSafef(offsetToCenter.x, worldPos.y) > 0.7f) {
    //            tile.topLayer = flowers;
    //        }
    //        else if (fNoise < 0.1f && Random::getThreadSafef(offsetToCenter.x, worldPos.y) > 0.5f) {
    //            tile.topLayer = flower;
    //        }
    //        else if (Random::getThreadSafef(offsetToCenter.x, offsetToCenter.y) > 0.9995f) {
    //            tile.topLayer = largeBush;
    //        }
    //        if (grass && Random::getThreadSafef(offsetToCenter.x, worldPos.y) > 0.02f) {
    //            *grass = 1;
    //        }
    //    }
    //}
    //else {
    //    float r = Random::getThreadSafef(offsetToCenter.x, worldPos.y);
    //    if (r > 0.6f) {
    //        tile.topLayer = hugeTree;
    //    }
    //}
    return tile;
}

void ChunkGenerator::GenerateChunk(Chunk& chunk, WorldGrid& worldGrid, const HeightmapPatchData* heightData) {

    PreciseTimer timer;

    // Allocate tiles if needed
    if (!chunk.mTiles.size()) {
        chunk.allocateTiles();
    }
    const ChunkID& id = chunk.getChunkID();

    const f32v2& chunkPosWorld = chunk.getWorldPos();
    f32 maxHeight = 1.0f;
    for (ui32 y = 0; y < CHUNK_WIDTH; ++y) {
        for (ui32 x = 0; x < CHUNK_WIDTH; ++x) {
            const f32v2 tilePosWorld(x + chunkPosWorld.x, y + chunkPosWorld.y);
            f32 height = worldGrid.computeCenterHeightAtTile(heightData->data, TileIndex(x, y));
            ui8 grass = 0;
            Tile tile = GenerateTileAtPos(tilePosWorld, height, &grass);
            if (tile.baseZPosition + 1.0f > maxHeight) {
                maxHeight = tile.baseZPosition + 1.0f;
            }
            TileIndex index(x, y);
            chunk.setTileFromGeneration(index, std::move(tile));
            chunk.mGrass[index] = grass;
        }
    }
    // TODO: uhhhh?
    // TODO: use heightData.bounding sphere?
    chunk.mAABB.height = maxHeight + 1.0f - chunk.mAABB.z; // Subtracting Z because we want to add the depth underground to the total height

    //std::cout << "Chunk generated in " << timer.stop() << " ms\n";
}