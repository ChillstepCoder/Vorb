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
    static TileID pineTree = TileRepository::getTile("tree_pine");

    constexpr f32 MAX_GRASS_HEIGHT = 16.0f;
    constexpr f32 MAX_TREE_HEIGHT = 30.0f;

    Tile tile(TILE_ID_NONE, TILE_ID_NONE, TILE_ID_NONE);
    f32v2 offsetToCenter(
        worldPos.x - WorldData::WORLD_CENTER.x,
        worldPos.y - WorldData::WORLD_CENTER.y
    );

    if (height > 0.0f) {
        // Surface
        if (grass && height < MAX_GRASS_HEIGHT) {
            f32 fadeMult = glm::min((MAX_GRASS_HEIGHT - height) * 0.1f, 1.0f);
            if (Random::getThreadSafef(offsetToCenter.x, worldPos.y) * fadeMult > 0.04f) {
                *grass = 1;
            }
        }
        if (height < MAX_TREE_HEIGHT) {
            f32 fadeMult = glm::min((MAX_TREE_HEIGHT - height) * 0.1f, 1.0f);
            f32 treeNoise = sWorldGen.mForestNoise.compute(worldPos.x, worldPos.y);
            if (Random::getThreadSafef(worldPos.y, worldPos.x) < treeNoise * fadeMult) {
                tile.topLayer = pineTree;
            }
        }
    }
    tile.setBaseZPosition(height);

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
            const f32 baseZPos = tile.getBaseZPositionUncompressed();
            if (baseZPos + 1.0f > maxHeight) {
                maxHeight = baseZPos + 1.0f;
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