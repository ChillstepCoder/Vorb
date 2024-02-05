#include "stdafx.h"
#include "ChunkGenerator.h"

#include "definitions/BiomeDef.h"

#include "world/biome/BiomeGrid.h"
#include "world/Chunk.h"
#include "math/Noise.h"
#include "math/Random.h"

#include "world/World.h"
#include "world/WorldDefaults.h"
#include "world/IHeightmapGrid.h"
#include "world/Region.h"
#include "world/chunk/SimChunkTileGrid.h"
#include "resources/TileRepository.h"

#include "generation/NoiseFunction.hpp"
#include "generation/WorldGenerationData.h"
#include "generation/TileDistributionSampler.h"

#include "util/TilingVoronoiMap.h"


ChunkGenerator::ChunkGenerator(World& world) : mWorld(world) {
    mWorldCenter = f32v2(mWorld.getWidthTiles() * 0.5f);
    mVoronoiMap = std::make_unique<TilingVoronoiMap>(256, 16);
    mSpatialGrid = std::make_unique<SpatialGrid2D>(CHUNK_WIDTH, mWorld.getWidthChunks());
}

ChunkGenerator::~ChunkGenerator() {

}

Tile ChunkGenerator::generateTileAtPos(i32v2 worldPos, f32 height, f32v3 normal, const BiomeDef* biomeDef) {

    // TODO: Blend density somehow? Density gradient calculated from neighbors + bilinear interpolation?
    const f32 DENSITY = 1.0f;

    Tile tile;
    int categoryIndex = 0; // For logging
    for (const OptimizedBiomeTileGenCategoryData& category : biomeDef->tileGenerationData) {
        if (!category.distributionPtr || !category.tiles.size()) [[unlikely]] {
            LOG_ERROR("Biome category {} for biome {} is missing distribution or tiles.", categoryIndex, biomeDef->displayName);
            ++categoryIndex;
            continue;
        }
            if (normal.z <= category.slopeRange.x && normal.z >= category.slopeRange.y) {
                if (TileDistributionSampler::sample(*category.distributionPtr, worldPos, DENSITY, category.probabilityMult)) {
                    const f32 randomRoll = Random::getThreadSafef(worldPos.y, worldPos.x);
                    for (auto& possibleTile : category.tiles) {
                        if (randomRoll <= possibleTile.weightThreshold) {
                            tile.mainLayer = possibleTile.tileId;

                            // Select variant
                            if (possibleTile.variantCount) {
                                f32 variantRoll = 0.0f;
                                switch (possibleTile.variantSelectionType) {
                                    case TileVariantSelectionType::Random: {
                                        variantRoll = Random::getThreadSafef(worldPos.y, worldPos.x);
                                        break;
                                    }
                                    case TileVariantSelectionType::Voronoi: {
                                        const i32v2 point = mVoronoiMap->getVoronoiPointAtTile(worldPos, 1.0f);
                                        variantRoll = Random::getThreadSafef(point.x, point.y);
                                        break;
                                    }
                                }

                                for (size_t i = possibleTile.variantStartIndex; i < possibleTile.variantStartIndex + possibleTile.variantCount; ++i) {
                                    const VariantWithWeightThreshold& variant = category.allVariants[i];
                                    if (variantRoll <= variant.weightThreshold) {
                                        tile.mainLayerVariant = variant.tileVariant;
                                        break;
                                    }
                                }
                            }
                            static_assert(e_count(TileVariantSelectionType) == 2);

                            return tile;
                        }
                    }
                }
            }
        ++categoryIndex;
    }
    return tile;
}

void ChunkGenerator::generateChunk(Chunk& chunk) {
    SimChunkTileGrid& simGrid = chunk.getWorld().getSimTileGrid();
    const SimChunkTileContainer& simData = simGrid.getChunk(chunk.getChunkID());
    TileRepository& tileRepo = TileRepository::get();
    IHeightmapGrid& heightGrid = chunk.getWorld().getHeightmapGrid();
    const i32v2 chunkPosWorld = chunk.getWorldPos();

    // Allocate tiles if needed
    chunk.mTileContainer->allocateData();

    if (simData.state != SimChunkTileContainerState::Allocated) {
        // TODO: Need to still generate tiles in ocean and stuff
        chunk.mAABB.height = 10;
        return;
    }

    std::shared_lock lock(simData.mutex);
    assert(simData.data);
    SimChunkTileData& simChunkTileData = *simData.data;
    std::vector<Tile>& tiles = chunk.mTileContainer->mTiles;
    for (auto& [index, data] : simChunkTileData.tileIndexToTileData) {

        const ui32 x = index & TILE_INDEX_X_MASK;
        const ui32 y = index >> TILE_INDEX_Y_SHIFT;
        tiles[index].groundZOffset = heightGrid.computeCenterHeightAtTile<true>(chunkPosWorld + i32v2(x, y));
        tiles[index].mainLayer = data.tileId;
        tiles[index].mainLayerVariant = data.variant;

        // TODO: Not ideal
        //const NavBlockerType navBlockerType = tileRepo.getLoadedOrUnloadedAsset(data.tileId).navBlockerType;
        //if (navBlockerType != NavBlockerType::NONE) {
        //    // Set blocked flags and erase tile if failed
        //    if (!chunk.mTileContainer->tryBlockAdjTilesFromGeneration(index, navBlockerType)) {
        //        tiles[index].mainLayer = TILE_ID_NONE; // Clear the main layer since it wont fit
        //        tiles[index].mainLayerVariant = 0;
        //    }
        //}
    }

    // Copy walls
    chunk.mTileContainer->mTileWallsContainer = simChunkTileData.tileWalls;
    chunk.mAABB.height = 50; // ???
}


void ChunkGenerator::generateChunkOld(Chunk& chunk) {
    PROFILE_FUNCTION();

    TileRepository& tileRepo = TileRepository::get();
    IHeightmapGrid& heightGrid = chunk.getWorld().getHeightmapGrid();
    BiomeGrid& biomeGrid = chunk.getWorld().getBiomeGrid();

    // Allocate tiles if needed
    chunk.mTileContainer->allocateData();
    ChunkID id = chunk.getChunkID();
    const i32v2 chunkPosWorld = chunk.getWorldPos();

    // Cache all center heights
    f32 centerHeights[CHUNK_SIZE];
    f32v3 centerNormals[CHUNK_SIZE];
    for (ui32 i = 0; i < CHUNK_SIZE; ++i) {
        const ui32 x = i & TILE_INDEX_X_MASK;
        const ui32 y = i >> TILE_INDEX_Y_SHIFT;
        centerHeights[i] = heightGrid.computeCenterHeightAndNormalAtTile<true>(chunkPosWorld + i32v2(x, y), &centerNormals[i]);
    }

    // Large objects
    /*for (ui32 i = 0; i < CHUNK_SIZE; ++i) {
        TryGenerateLargeObjectAtPoint()
    }*/

    // Small objects

    f32 maxHeight = 1.0f;
    std::vector<Tile>& tiles = chunk.mTileContainer->mTiles;
    for (ui32 i = 0; i < CHUNK_SIZE; ++i) {
        const ui32 x = i & TILE_INDEX_X_MASK;
        const ui32 y = i >> TILE_INDEX_Y_SHIFT;
        const f32v2 tilePosWorld(x + chunkPosWorld.x, y + chunkPosWorld.y);
        const f32 height = centerHeights[i];
        TileGrass grass;
        // TODO: Biome specific
        const BiomeDef* biomeDef = biomeGrid.getBiomeDefAtPoint(tilePosWorld);
        generateTileGrass(tilePosWorld, height, &grass); // TODO: USE BIOME
        Tile tile = generateTileAtPos(tilePosWorld, height, centerNormals[i], biomeDef);
        tile.groundZOffset = height;
        const f32 baseZPos = height;
        if (baseZPos + 1.0f > maxHeight) {
            maxHeight = baseZPos + 1.0f;
        }
        // TODO: Bit array instead of full tiledata lookup for cache friendlyness
        if (tile.mainLayer != TILE_ID_NONE) {
            const NavBlockerType navBlockerType = tileRepo.getLoadedOrUnloadedAsset(tile.mainLayer).navBlockerType;
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

void ChunkGenerator::generateSimChunk(SimChunkTileContainer& chunk, World& world) {
    PROFILE_FUNCTION();
    PreciseTimer timer;

    TileRepository& tileRepo = TileRepository::get();
    IHeightmapGrid& heightGrid = world.getHeightmapGrid();
    BiomeGrid& biomeGrid = world.getBiomeGrid();
    SimChunkTileGrid& simGrid = world.getSimTileGrid();

    // Allocate tiles if needed
    ChunkID id = chunk.chunkId;
    if (chunk.allocate()) {
        // FOR MEMORY TRACKING ONLY
        simGrid.onNewChunkAllocated();
    }
    SimChunkTileData& chunkData = *chunk.data;
    const i32v2 chunkPosWorld = mSpatialGrid->getWorldPosXYFromID(id);

    ui32 totalTiles = 0;
    for (ui32 i = 0; i < CHUNK_SIZE; ++i) {
        const ui32 x = i & TILE_INDEX_X_MASK;
        const ui32 y = i >> TILE_INDEX_Y_SHIFT;
        const f32v2 tilePosWorld(x + chunkPosWorld.x, y + chunkPosWorld.y);
        f32v3 normal;
        const f32 height = heightGrid.computeCenterHeightAndNormalAtTile<true>(chunkPosWorld + i32v2(x, y), &normal);
        Tile tile = generateTileAtPos(tilePosWorld, height, normal, biomeGrid.getBiomeDefAtPoint(tilePosWorld));

        if (tile.mainLayer != TILE_ID_NONE) {

            auto&& it = chunkData.tileQuantities.find(tile.mainLayer);
            if (it == chunkData.tileQuantities.end()) [[unlikely]] {
                chunkData.tileQuantities.emplace(tile.mainLayer, 1);
            }
            else {
                ++it->second;
            }

            SimTileData tileData;
            tileData.tileId = tile.mainLayer;
            tileData.variant = tile.mainLayerVariant;
            chunkData.tileIndexToTileData.emplace((ui16)i, tileData);
            ++totalTiles;
            chunkData.tileStates[i] = SimTileState::Tile;
        }
    }

    // Shrink memory
    chunkData.tileIndexToTileData.shrink_to_fit();
    chunkData.tileQuantities.shrink_to_fit();

    LOG_DEBUG("Generated simchunk in {} ms {} {} {}", timer.stop(), totalTiles, (f32)totalTiles / CHUNK_SIZE, chunkData.tileIndexToTileData.size());
}

void ChunkGenerator::generateTileGrass(i32v2 worldPos, f32 height, TileGrass* grass) {
    static constexpr TileGrassID defaultGrass = 0; // TODO: DIFFERENT

    constexpr f32 MAX_GRASS_HEIGHT = 45.0f;
    if (height < MAX_GRASS_HEIGHT) {
        //f32 fadeMult = glm::min((MAX_GRASS_HEIGHT - height) * 0.1f, 1.0f);

        constexpr f32 GRASS_SCALE = 2.0f;
        constexpr f32 GRASS_OFFSET = 0.45f;
        const f32 grassNoise = mGenerationData.mGrassNoise.compute(worldPos.x, worldPos.y);
        const ui8 density = (ui8)glm::clamp(glm::round((grassNoise * GRASS_SCALE + GRASS_OFFSET) * 255.0f), 0.0f, 255.0f);
        grass->grassIDs[0] = defaultGrass;
        grass->densities[0] = density;
    }
}