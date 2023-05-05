#include "stdafx.h"
#include "WorldGenerator.h"

#include "world/Chunk.h"
#include "math/Noise.h"
#include "math/Random.h"

#include "world/IWorld.h"
#include "world/WorldData.h"
#include "world/IHeightmapGrid.h"
#include "world/Region.h"
#include "resources/TileRepository.h"

#include "generation/NoiseFunction.hpp"
#include "generation/WorldGenerationData.h"


WorldGenerator::WorldGenerator(IWorld& world) : mWorld(world) {
    mWorldCenter = f32v2(mWorld.getWidthTiles() * 0.5f);
}

WorldGenerator::~WorldGenerator() {

}

Tile WorldGenerator::generateTileAtPos(const f32v2& worldPos, f32 height, TileGrass* grass /*= nullptr*/) {
    assert(grass);
    // TODO: This seems wrong
    static TileID baseTree = TileRepository::getTile(StrToken("tree_a"));
    static TileID pineTree = TileRepository::getTile(StrToken("tree_pine"));
    static TileID bushMed = TileRepository::getTile(StrToken("bush_med"));
    static TileID bush2 = TileRepository::getTile(StrToken("bush_g"));
    static TileGrassID defaultGrass = 0; // TODO: DIFFERENT

    constexpr f32 MAX_GRASS_HEIGHT = 16.0f;
    constexpr f32 MAX_TREE_HEIGHT = 100.0f;

    Tile tile(TILE_ID_NONE, TILE_ID_NONE, TILE_ID_NONE);
    f32v2 offsetToCenter(
        worldPos.x - mWorldCenter.x,
        worldPos.y - mWorldCenter.y
    );

    if (height > 0.0f) {
        // Surface
        if (height < MAX_GRASS_HEIGHT) {
            //f32 fadeMult = glm::min((MAX_GRASS_HEIGHT - height) * 0.1f, 1.0f);

            constexpr f32 GRASS_SCALE = 2.0f;
            constexpr f32 GRASS_OFFSET = 0.45f;
            const f32 grassNoise = mGenerationData.mGrassNoise.compute(worldPos.x, worldPos.y);
            const ui8 density = (ui8)glm::clamp(glm::round((grassNoise * GRASS_SCALE + GRASS_OFFSET) * 255.0f), 0.0f, 255.0f);
            grass->grassIDs[0] = defaultGrass;
            grass->densities[0] = density;
        }
        if (height < MAX_TREE_HEIGHT) {
            f32 fadeMult = glm::min((MAX_TREE_HEIGHT - height) * 0.01f, 1.0f);
            f32 treeNoise = mGenerationData.mForestNoise.compute(worldPos.x, worldPos.y);
            constexpr f32 TREE_DENSITY = 0.1f;
            constexpr f32 BUSH_DENSITY = 0.015f;
            if (Random::getThreadSafef(worldPos.y, worldPos.x) < treeNoise * TREE_DENSITY * fadeMult) {
                if (Random::getThreadSafef(worldPos.x * -90.353f, worldPos.y * 5.25f) < 0.5f) {
                    tile.mainLayer = baseTree;
                }
                else {
                    tile.mainLayer = pineTree;
                }
                *grass = TileGrass();
            }
            else if (Random::getThreadSafef(worldPos.x, worldPos.y * 4041.0f) < BUSH_DENSITY) {
                tile.mainLayer = bush2;
                *grass = TileGrass();
            }
            else if (Random::getThreadSafef(worldPos.x * 4021.0f, worldPos.y * 22.0f) < BUSH_DENSITY * 0.7f) {
                tile.mainLayer = bushMed;
                *grass = TileGrass();
            }
        }
    }
    //if (height > 0.3) {
    //    //tile.groundLayer = rock1;
    //    // Mountains
    //    tile.groundZPosition = (ui16)((height - 0.3) / 0.004) + 1u;
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


    tile.groundZOffset = height;

    return tile;
}

void WorldGenerator::generateChunk(Chunk& chunk, f32* heightData) {
    PROFILE_FUNCTION();

    // Allocate tiles if needed
    chunk.mTileContainer->allocateData();
    const ChunkID& id = chunk.getChunkID();

    // Cache all min heights
    f32 centerHeights[CHUNK_SIZE];
    for (ui32 i = 0; i < CHUNK_SIZE; ++i) {
        const ui32 x = i & TILE_INDEX_X_MASK;
        const ui32 y = i >> TILE_INDEX_Y_SHIFT;
        centerHeights[i] = chunk.getWorld().getHeightmapGrid().computeCenterHeightAtTile(heightData, chunk.mTileContainer->getTileSpatialGrid().getWorldPos2D() + i32v2(x, y));
    }

    // Large objects
    /*for (ui32 i = 0; i < CHUNK_SIZE; ++i) {
        TryGenerateLargeObjectAtPoint()
    }*/

    // Small objects

    const f32v2 chunkPosWorld = chunk.getWorldPos();
    f32 maxHeight = 1.0f;
    std::vector<Tile>& tiles = chunk.mTileContainer->mTiles;
    for (ui32 i = 0; i < CHUNK_SIZE; ++i) {
        const ui32 x = i & TILE_INDEX_X_MASK;
        const ui32 y = i >> TILE_INDEX_Y_SHIFT;
        const f32v2 tilePosWorld(x + chunkPosWorld.x, y + chunkPosWorld.y);
        const f32 height = centerHeights[y * CHUNK_WIDTH + x];
        TileGrass grass;
        Tile tile = generateTileAtPos(tilePosWorld, height, &grass);
        const f32 baseZPos = tile.getGroundZOffset();
        if (baseZPos + 1.0f > maxHeight) {
            maxHeight = baseZPos + 1.0f;
        }
        // TODO: Bit array instead of full tiledata lookup for cache friendlyness
        if (tile.mainLayer != TILE_ID_NONE) {
            const NavBlockerType navBlockerType = TileRepository::getTileData(tile.mainLayer).navBlockerType;
            if (navBlockerType != NavBlockerType::NONE) {
                // Set blocked flags
                if (chunk.mTileContainer->tryBlockAdjTilesFromGeneration(i, navBlockerType)) {
                    TileFlagType prevFlags = tiles[i].tileFlags.getBits();
                    tiles[i] = std::move(tile);
                    tiles[i].tileFlags.setBits((TileFlags)prevFlags);
                }
                else {
                    tile.mainLayer = TILE_ID_NONE; // Clear the main layer since it wont fit
                    TileFlagType prevFlags = tiles[i].tileFlags.getBits();
                    tiles[i] = std::move(tile);
                    tiles[i].tileFlags.setBits((TileFlags)prevFlags);
                }
            }
            else {
                TileFlagType prevFlags = tiles[i].tileFlags.getBits();
                tiles[i] = std::move(tile);
                tiles[i].tileFlags.setBits((TileFlags)prevFlags);
            }
        }
        else {
            // Make sure we don't obliterate blocked flags
            TileFlagType prevFlags = tiles[i].tileFlags.getBits();
            tiles[i] = std::move(tile);
            tiles[i].tileFlags.setBits((TileFlags)prevFlags);
        }
        chunk.mGrass[i] = grass;
    }
    // TODO: uhhhh?
    // TODO: use heightData.bounding sphere?
    chunk.mAABB.height = (i32)floor(maxHeight + 1.0f - chunk.mAABB.z); // Subtracting Z because we want to add the depth underground to the total height
}

f32 WorldGenerator::getTerrainHeightAtPos(const f32v2& worldPos) {
    // Base height
    f64 height = mGenerationData.mBaseNoise.compute((f64)worldPos.x, (f64)worldPos.y);

    f32v2 offsetToCenter(
        worldPos.x - mWorldCenter.x,
        worldPos.y - mWorldCenter.y
    );

    //  TODO: Precompute and interpolate, can cubic interpolate and others
    f64 distanceFromCenter2 = glm::length2(offsetToCenter);

    // Preturb the outline via noise
    distanceFromCenter2 += mGenerationData.CONTINENT_OUTLINE_SCALE * mGenerationData.mContinentOutlineNoise.compute(offsetToCenter.x, offsetToCenter.y);

    // Outline check
    if (distanceFromCenter2 > mGenerationData.CONTINENT_RADIUS_SQ) {
        // Ocean
        height -= (distanceFromCenter2 - mGenerationData.CONTINENT_RADIUS_SQ) * 0.0000001;
    }
    else {
        // Continent internals
        f64 lerp = (mGenerationData.CONTINENT_RADIUS_SQ - distanceFromCenter2) * 0.00000001;
        // Mountains
        f64 mountainDist = mGenerationData.mMountainsDistNoise.compute((f64)worldPos.x, (f64)worldPos.y);
        if (mountainDist > 0.0) {
            f64 mountain = mGenerationData.mMountainsNoise.compute((f64)worldPos.x, (f64)worldPos.y);
            height += lerp * mountain * glm::min(mountainDist, 1.0);
        }
    }
    return glm::clamp((f32)height, MIN_WORLD_GEN_HEIGHT, MAX_WORLD_GEN_HEIGHT);
}
