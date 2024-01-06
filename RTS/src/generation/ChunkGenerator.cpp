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
#include "resources/TileRepository.h"

#include "generation/NoiseFunction.hpp"
#include "generation/WorldGenerationData.h"

#include "util/TilingVoronoiMap.h"


ChunkGenerator::ChunkGenerator(World& world) : mWorld(world) {
    mWorldCenter = f32v2(mWorld.getWidthTiles() * 0.5f);
    mVoronoiMap = std::make_unique<TilingVoronoiMap>(256, 16);
}

ChunkGenerator::~ChunkGenerator() {

}

Tile ChunkGenerator::generateTileAtPos(f32v2 worldPos, f32 height, f32v3 normal, TileGrass* grass, const BiomeDef* biomeDef) {
    assert(grass);

    if (biomeDef) {

        // TODO: Data driven generation
        switch (biomeDef->uniqueId) {
            case BiomeUniqueID::Ocean:
                return Tile();
            case BiomeUniqueID::Plains:
                return generateTilePlains(worldPos, height, grass, biomeDef);
            case BiomeUniqueID::Mountains:
                return generateTileMountains(worldPos, height, grass, biomeDef);
            case BiomeUniqueID::Forest:
                return generateTileForests(worldPos, height, grass, biomeDef);
            case BiomeUniqueID::Hotsprings:
                return generateTileHotsprings(worldPos, height, normal, grass, biomeDef);
            default:
                break;

        }
        static_assert(e_count(BiomeUniqueID) == 13);
    }

    return Tile();
}

Tile ChunkGenerator::generateTilePlains(f32v2 worldPos, f32 height, TileGrass* grass, const BiomeDef* biomeDef) {
    TileRepository& tileRepo = TileRepository::get();
    static TileID baseTree = tileRepo.getTileID(CStrToken("tree_a"));
    static TileID pineTree = tileRepo.getTileID(CStrToken("tree_pine"));
    static TileID birchTree = tileRepo.getTileID(CStrToken("tree_birch"));
    static TileID bushMed = tileRepo.getTileID(CStrToken("bush_med"));
    static TileID bush2 = tileRepo.getTileID(CStrToken("bush_g"));

    generateTileGrass(worldPos, height, grass);

    constexpr f32 MAX_TREE_HEIGHT = 100.0f;
    Tile tile(TILE_ID_NONE, TILE_ID_NONE, TILE_ID_NONE);
    if (height < MAX_TREE_HEIGHT) {
        f32 fadeMult = glm::min((MAX_TREE_HEIGHT - height) * 0.01f, 1.0f);
        f32 treeNoise = mGenerationData.mForestNoise.compute(worldPos.x, worldPos.y);
        constexpr f32 TREE_DENSITY = 0.001f;
        constexpr f32 BUSH_DENSITY = 0.015f;
        if (Random::getThreadSafef(worldPos.y, worldPos.x) < treeNoise * TREE_DENSITY * fadeMult) {
            if (Random::getThreadSafef(worldPos.x * -90.353f, worldPos.y * 5.25f) < 0.42f) {
                tile.mainLayer = baseTree;
            }
            else if (Random::getThreadSafef(worldPos.x * 20.353f, worldPos.y * -54.25f) < 0.3f) {
                tile.mainLayer = birchTree;
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
    return tile;
}

Tile ChunkGenerator::generateTileMountains(f32v2 worldPos, f32 height, TileGrass* grass, const BiomeDef* biomeDef) {
    generateTileGrass(worldPos, height, grass);
    TileRepository& tileRepo = TileRepository::get();
    static TileID rockIds[5] = {
        tileRepo.getTileID(CStrToken("rock_xsmall_01")),
        tileRepo.getTileID(CStrToken("rock_small_01")),
        tileRepo.getTileID(CStrToken("rock_small_02")),
        tileRepo.getTileID(CStrToken("rock_med_01")),
        tileRepo.getTileID(CStrToken("rock_med_02"))
    };
    constexpr f32 TREE_DENSITY = 0.1f;

    Tile tile(TILE_ID_NONE, TILE_ID_NONE, TILE_ID_NONE);
    if (Random::getThreadSafef(worldPos.y, worldPos.x) < TREE_DENSITY) {
        tile.mainLayer = rockIds[rand() % 5];
    }

    return tile;
}

Tile ChunkGenerator::generateTileForests(f32v2 worldPos, f32 height, TileGrass* grass, const BiomeDef* biomeDef) {
    TileRepository& tileRepo = TileRepository::get();
    static TileID baseTree = tileRepo.getTileID(CStrToken("tree_a"));
    static TileID pineTree = tileRepo.getTileID(CStrToken("tree_pine"));
    static TileID birchTree = tileRepo.getTileID(CStrToken("tree_birch"));
    static TileID bushMed = tileRepo.getTileID(CStrToken("bush_med"));
    static TileID bush2 = tileRepo.getTileID(CStrToken("bush_g"));

    generateTileGrass(worldPos, height, grass);

    constexpr f32 MAX_TREE_HEIGHT = 100.0f;
    Tile tile(TILE_ID_NONE, TILE_ID_NONE, TILE_ID_NONE);
    if (height < MAX_TREE_HEIGHT) {
        f32 fadeMult = glm::min((MAX_TREE_HEIGHT - height) * 0.01f, 1.0f);
        constexpr f32 TREE_DENSITY = 0.05f;
        constexpr f32 BUSH_DENSITY = 0.013f;
        if (Random::getThreadSafef(worldPos.y, worldPos.x) < TREE_DENSITY * fadeMult) {
            if (height > 0.1f) {
                if (Random::getThreadSafef(worldPos.x * -90.353f, worldPos.y * 5.25f) < 0.42f) {
                    tile.mainLayer = baseTree;
                }
                else if (Random::getThreadSafef(worldPos.x * 20.353f, worldPos.y * -54.25f) < 0.3f) {
                    tile.mainLayer = birchTree;
                }
                else {
                    tile.mainLayer = pineTree;
                }
                *grass = TileGrass();
            }
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
    return tile;
}

Tile ChunkGenerator::generateTileHotsprings(f32v2 worldPos, f32 height, f32v3 normal, TileGrass* grass, const BiomeDef* biomeDef) {

    TileRepository& tileRepo = TileRepository::get();
    static TileID hotspring01 = tileRepo.getTileID(CStrToken("hotspring_01"));
    static TileID hotspringCh01 = tileRepo.getTileID(CStrToken("hotspring_ch01"));
    static TileID hotspringhero = tileRepo.getTileID(CStrToken("hotspringhero"));


    Tile tile(TILE_ID_NONE, TILE_ID_NONE, TILE_ID_NONE);
    constexpr f32 TREE_DENSITY = 0.005f;
    if (Random::getThreadSafef(worldPos.y, worldPos.x) < TREE_DENSITY) {
        if (Random::getThreadSafef(worldPos.x * 10.353f, worldPos.y * -554.25f) < 0.1f) {
            tile.mainLayer = hotspringhero;
        }
        else if (Random::getThreadSafef(worldPos.x * 20.353f, worldPos.y * -54.25f) < 0.2f) {
            tile.mainLayer = hotspringhero;
        }
        *grass = TileGrass();
    }
    else if (normal.z < 0.65f) {
        if (Random::getThreadSafef(worldPos.x * 10.353f, worldPos.y * -554.25f) < 0.0125f) {
            tile.mainLayer = hotspringhero;
        } else if (Random::getThreadSafef(worldPos.y, worldPos.x) < 0.025f) {
            tile.mainLayer = hotspringhero;
            *grass = TileGrass();
        }
    }

    if (tile.mainLayer == hotspringhero) {
        i32v2 point = mVoronoiMap->getVoronoiPointAtTile(i32v2(worldPos), 1.0f);
        tile.mainLayerVariant = Random::getThreadSafe(point.x, point.y) % 6;
    }

    //generateTileGrass(worldPos, height, grass);

    return tile;
}

void ChunkGenerator::generateChunk(Chunk& chunk) {
    PROFILE_FUNCTION();

    TileRepository& tileRepo = TileRepository::get();
    IHeightmapGrid& heightGrid = chunk.getWorld().getHeightmapGrid();
    BiomeGrid& biomeGrid = chunk.getWorld().getBiomeGrid();

    // Allocate tiles if needed
    chunk.mTileContainer->allocateData();
    const ChunkID& id = chunk.getChunkID();

    // Cache all center heights
    f32 centerHeights[CHUNK_SIZE];
    f32v3 centerNormals[CHUNK_SIZE];
    for (ui32 i = 0; i < CHUNK_SIZE; ++i) {
        const ui32 x = i & TILE_INDEX_X_MASK;
        const ui32 y = i >> TILE_INDEX_Y_SHIFT;
        centerHeights[i] = heightGrid.computeCenterHeightAndNormalAtTile<true>(chunk.mTileContainer->getTileSpatialGrid().getWorldPos2D() + i32v2(x, y), &centerNormals[i]);
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
        const f32 height = centerHeights[i];
        TileGrass grass;
        Tile tile = generateTileAtPos(tilePosWorld, height, centerNormals[i], &grass, biomeGrid.getBiomeDefAtPoint(tilePosWorld));
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

void ChunkGenerator::generateTileGrass(f32v2 worldPos, f32 height, TileGrass* grass) {
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