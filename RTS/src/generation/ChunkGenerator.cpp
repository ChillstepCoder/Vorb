#include "stdafx.h"
#include "ChunkGenerator.h"

#include "definitions/BiomeDef.h"

#include "world/biome/BiomeGrid.h"
#include "world/Chunk.h"

#include "world/World.h"
#include "world/IHeightmapGrid.h"
#include "world/chunk/SimChunkGrid.h"
#include "world/road/TerrainSurfaceGrid.h"
#include "resources/TileRepository.h"

#include "generation/NoiseFunction.hpp"
#include "generation/WorldGenerationData.h"
#include "generation/TileDistributionSampler.h"

#include "rendering/renderdata/WorldRenderDataManager.h"
#include "rendering/mesh/TileContainerMeshManager.h"

//#include "debugging/DebugRenderer.h"

#include "util/BitArray.h"
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
                                    tile.variant = variant.tileVariant;
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


void ChunkGenerator::generateChunkFromSimChunk(Chunk& chunk, const BitArray& buildingFootprint) {
    World& world = chunk.getWorld();
    SimChunkGrid& simGrid = world.getSimChunkGrid();
    SimChunk& simData = simGrid.getChunkForGeneration(chunk.getChunkID());

    if (world.isEditorWorld()) [[unlikely]] {
        // Need to generate the sim chunk first on editor worlds, as we did not generate history
        generateSimChunk(simData, world);
    }

    TileRepository& tileRepo = TileRepository::get();
    IHeightmapGrid& heightGrid = world.getHeightmapGrid();
    TerrainSurfaceGrid& surfaceGrid = world.getTerrainSurfaceGrid();
    const i32v2 chunkPosWorld = chunk.getWorldPos();

    // Allocate tiles if needed
    chunk.mTileContainer->allocateData();

    // Indicates an ocean chunk
    if (simData.mState != SimChunkState::Allocated) {
        // TODO: Need to still generate tiles in ocean and stuff
        chunk.mAABB.height = 10;
        return;
    }

    // Keep track of which tiles are models
    TileIndex modelTiles[CHUNK_SIZE];
    i32 numModelTiles = 0;
    TileIndex proceduralTiles[CHUNK_SIZE];
    i32 numProceduralTiles = 0;

    f32 minHeight = FLT_MAX;
    f32 maxHeight = -FLT_MAX;

    // Set all tile ground positions, mark building locations, and generate grass
    std::vector<Tile>& tiles = chunk.mTileContainer->mTiles;
    for (i32 index = 0; index < CHUNK_SIZE; ++index) {
        const TileCoord coord(chunkPosWorld + i32v2(index & TILE_INDEX_X_MASK, index >> TILE_INDEX_Y_SHIFT));
        tiles[index].groundZOffset = heightGrid.computeCenterHeightAtTile<true>(coord);
        if (tiles[index].groundZOffset < minHeight) {
            minHeight = tiles[index].groundZOffset;
        }
        else if (tiles[index].groundZOffset > maxHeight) {
            maxHeight = tiles[index].groundZOffset;
        }
        if (!buildingFootprint.isEmpty() && buildingFootprint.getBit(index)) {
            tiles[index].tileFlags.setBit(TileFlags::IS_BLOCKED_BY_BUILDING);
        }
        else if (surfaceGrid.getSurfacePoint<true>(DTileCoord(coord)).baseType == TerrainSurfaceType::None) {
            chunk.mGrass[index] = generateTileGrass(coord.v, tiles[index].groundZOffset, 1.0f /*intensitymult*/);
        }
    }
    // Read+write lock
    {
        std::lock_guard lock(simData.mMutex);
        assert(simData.mTileData);
        SimChunkTileData& simChunkTileData = *simData.mTileData;
        auto& tileIndexToTileData = simChunkTileData.tileIndexToTileData;
        for (auto&& it = tileIndexToTileData.begin(); it != tileIndexToTileData.end();) {
            auto& [index, data] = *it;
            Tile& tile = tiles[index];
            assert(isTileValid(data.tileId));
            if (tile.tileFlags.isBitSet(TileFlags::IS_BLOCKED_BY_BUILDING)) {
                // Remove blocked tile from sim layer and do not add to this chunk
                it = simChunkTileData.removeTileDuringIter(it);
            }
            else {
                tiles[index].mainLayer = data.tileId;
                tiles[index].variant = data.variant;
                tiles[index].typeDataCopy = data.typeData;
                chunk.mGrass[index] = TileGrass();
                ++it;

                if (tileRepo.getTileModelID(data.tileId) != INVALID_MODEL_ID) {
                    modelTiles[numModelTiles++] = index;
                }
                else {
                    proceduralTiles[numProceduralTiles++] = index;
                }
            }
        }

        // Copy walls
        chunk.mTileContainer->mTileWallsContainer = simChunkTileData.tileWalls;
    }

    // We need some class that is in charge of creating the orientations, sizes, and positions for our models,
    // sending them to both the model manager and the physics world (And maybe the nav graph)
    // Handle is a ContainerID and a TileIndex as we have no layers. Only one model may exist per tile right now.
    // Then each manager just has to listen for update events for new tiles, removed tiles, or container destruction
    // A good name for this class is TileContainerMeshManager
    // We could potentially collapse all the update logic into addPhysics/MeshesForTileModels(std::span<tiles>)
    //x;


    if (numModelTiles) {
        mWorld.getRenderDataManager().getTileContainerMeshManager().initModelsForTileContainer({ modelTiles, (ui32)numModelTiles }, *chunk.mTileContainer);
        ++chunk.mTileContainer->mPhysicsInitWaiting;
        ++chunk.mTileContainer->mMeshInitWaiting;
    }
    if (numProceduralTiles) {
        assert(false); // Build this
        //++chunk.mTileContainer->mPhysicsInitWaiting;
        //++chunk.mTileContainer->mMeshInitWaiting;
    }
    constexpr f32 AABB_Z_PADDING = 10.0f;
    chunk.mAABB.pos.z = minHeight - AABB_Z_PADDING;
    chunk.mAABB.height = (maxHeight - minHeight) + AABB_Z_PADDING;
}


void ChunkGenerator::generateSimChunk(SimChunk& chunk, World& world) {
    PROFILE_FUNCTION();
    PreciseTimer timer;

    TileRepository& tileRepo = TileRepository::get();
    IHeightmapGrid& heightGrid = world.getHeightmapGrid();
    BiomeGrid& biomeGrid = world.getBiomeGrid();
    SimChunkGrid& simGrid = world.getSimChunkGrid();

    // Allocate tiles if needed
    ChunkID id = chunk.mChunkID;
    if (chunk.allocate()) {
        // FOR MEMORY TRACKING ONLY
        simGrid.onNewChunkAllocated();
    }
    SimChunkTileData& chunkData = *chunk.mTileData;
    const i32v2 chunkPosWorld = mSpatialGrid->getPosFromID(id);

    assert(chunkData.harvestables.empty());
    assert(chunkData.tileIndexToTileData.empty());

    ui32 totalTiles = 0;
    for (ui32 i = 0; i < CHUNK_SIZE; ++i) {
        const ui32 x = i & TILE_INDEX_X_MASK;
        const ui32 y = i >> TILE_INDEX_Y_SHIFT;
        const TileCoord tilePosWorld(x + chunkPosWorld.x, y + chunkPosWorld.y);
        f32v3 normal;
        const f32 height = heightGrid.computeCenterHeightAndNormalAtTile<true>(tilePosWorld, &normal);
        const BiomeDef* def = biomeGrid.getBiomeDefAtPoint(tilePosWorld.v);
        assert(def);
        Tile tile = generateTileAtPos(tilePosWorld.v, height, normal, def);

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
            tileData.variant = tile.variant;
            chunkData.tileIndexToTileData.emplace((ChunkTileIndex)i, tileData);
            chunkData.debugValidateHarvestables();
            const TileDef& def = tileRepo.getLoadedOrUnloadedAsset(tile.mainLayer);
            if (def.harvestable != TileHarvestable::None) {
                chunkData.harvestables[def.harvestable].emplace_back(i);
            }
            switch (def.tileType) {
                case TileType::Default:
                    break;
                case TileType::Flora: {
                    const ui8 age = (ui8)Random::getThreadSafe(tilePosWorld.y, tilePosWorld.x) & 0xFF;
                    tileData.typeData = FloraTileData{ .age = age, .fruitAge = 0 };
                    break;
                }
                default:
                    assert(false);
                    break;
            }
            static_assert(e_count(TileType) == 2);
            ++totalTiles;
        }
    }

    // Shrink memory TODO: Rehash?
    //chunkData.tileIndexToTileData.shrink_to_fit();
    chunkData.tileQuantities.shrink_to_fit();
    chunkData.harvestables.shrink_to_fit();
    for (auto&& it : chunkData.harvestables) {
        it.second.shrink_to_fit();
    }

    chunkData.debugValidateHarvestables();

    //LOG_DEBUG("Generated simchunk in {} ms {} {} {}", timer.stop(), totalTiles, (f32)totalTiles / CHUNK_SIZE, chunkData.tileIndexToTileData.size());
}

TileGrass ChunkGenerator::generateTileGrass(i32v2 worldPos, f32 height, f32 intensityMult) {
    static constexpr TileGrassID defaultGrass = 0; // TODO: DIFFERENT
    static constexpr TileGrassID tmpSecondGrass = 1; // TODO: DIFFERENT
    
    TileGrass rv;
    constexpr f32 MAX_GRASS_HEIGHT = 45.0f;
    if (height < MAX_GRASS_HEIGHT && intensityMult > 0.0f) {
        //f32 fadeMult = glm::min((MAX_GRASS_HEIGHT - height) * 0.1f, 1.0f);

        constexpr f32 GRASS_SCALE = 2.0f;
        constexpr f32 GRASS_OFFSET = 0.45f;
        const f32 grassNoise = mGenerationData.mGrassNoise.compute(worldPos.x, worldPos.y);
        f32 baseGrassDensity = glm::round(grassNoise * GRASS_SCALE + GRASS_OFFSET);
        const ui8 density = (ui8)glm::clamp((baseGrassDensity * 255.0f), 0.0f, 255.0f);
        rv.grassIDs[0] = defaultGrass;
        rv.densities[0] = density * intensityMult;
        // TODO: TMP
        const ui8 density2 = (ui8)glm::clamp((glm::max((f32)baseGrassDensity, 0.0f) * (f32)mGenerationData.mGrassNoise.compute(worldPos.x + 2000.0f, worldPos.y + 2000.0f)) * 255.0f, 0.0f, 255.0f);
        rv.grassIDs[1] = tmpSecondGrass;
        rv.densities[1] = density2 * intensityMult;
    }
    return rv;
}