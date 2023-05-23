#include "stdafx.h"
#include "GrassMeshBuilderMethods.h"

#include "rendering/GrassBillboardMesh.h"
#include "rendering/ChunkGrassQuadtree.h"
#include "world/IWorld.h"
#include "world/Chunk.h"
#include "world/IHeightmapGrid.h"

#include "generation/IWorldGenerator.h"

#include "resources/ResourceManager.h"
#include "resources/TileGrassRepository.h"

#include "math/Random.h"

constexpr int GRASS_LOD_DETAIL[MAX_GRASS_DETAIL + 1][GRASS_QUADTREE_MAX_LOD] = {
    { 0, 0, 0, 0, 0 }, // 0
    { 0, 1, 1, 1, 1 }, // 1
    { 0, 1, 1, 2, 2 }, // 2
    { 0, 1, 1, 2, 3 }, // 3
    { 0, 1, 1, 3, 4 }, // 4
    { 0, 1, 1, 3, 5 }, // 5
    { 0, 1, 1, 4, 6 }, // 6
    { 0, 1, 2, 4, 7 }, // 7
    { 0, 1, 2, 5, 8 }, // 8
    { 0, 1, 2, 5, 9 }, // 9
    { 0, 1, 3, 6, 10 }, // 10
    { 0, 1, 3, 6, 11 }, // 11
    { 0, 1, 3, 6, 12 }, // 12
};
static_assert(MAX_GRASS_DETAIL == 12);

constexpr f32 GRASS_BLADE_WIDTHS[GRASS_QUADTREE_MAX_LOD] = {
    0.0f,
    1.00f,
    0.5f,
    0.12f,
    0.05f,
};

inline float smoothstep(float t) {
    return t * t * (3.0f - 2.0f * t);
}

constexpr f32 DIVIDE_MULT = 1.0f / 255.0f;

// Bilinear interpolation
// d2 d3
// d0 d1
f32 interpolateDensity(f32 d0, f32 d1, f32 d2, f32 d3, const f32v2& offsetFromd0) {
    // We interpolate from d0 to d1, and d2 to d3, then interpolate vertically to get our final value
    const f32 sx = smoothstep(offsetFromd0.x);
    const f32 sy = smoothstep(offsetFromd0.y);
    const f32 oneMinusXOffset = 1.0f - sx;
    const f32 r1 = d0 * oneMinusXOffset + d1 * sx;
    const f32 r2 = d2 * oneMinusXOffset + d3 * sx;
    return r1 * (1.0f - sy) + r2 * sy;
};

f32v2 getOffsetFromD0(f32 x, f32 y) {
    f32v2 rv;
    rv.x = x <= 0.5f ? x + 0.5f : x - 0.5f;
    rv.y = y <= 0.5f ? y + 0.5f : y - 0.5f;
    return rv;
};

float boundsCheckGetDensityMult(int sx, int sy, float xb, float yb, int widthTiles, float baseDensity, const TileGrass* grassDataArray, TileGrassID id) {
    constexpr auto boundsCheckGetDensity = [](int x, int y, int width, const TileGrass* grassData, TileGrassID id) -> f32 {
        if (x < 0 || y < 0 || x >= width || y >= width) return 0.0f;
        return grassData[y * width + x].getDensity(id) * DIVIDE_MULT;
    };

    const f32 d0 = boundsCheckGetDensity(sx, sy, widthTiles, grassDataArray, id);
    const f32 d1 = boundsCheckGetDensity(sx + 1, sy, widthTiles, grassDataArray, id);
    const f32 d2 = boundsCheckGetDensity(sx, sy + 1, widthTiles, grassDataArray, id);
    const f32 d3 = boundsCheckGetDensity(sx + 1, sy + 1, widthTiles, grassDataArray, id);
    // We use negative of our density if zero, so we get a nice transition instead of a hard edge (iamverysmart)
    return interpolateDensity(
        d0 > 0 ? d0 : -baseDensity,
        d1 > 0 ? d1 : -baseDensity,
        d2 > 0 ? d2 : -baseDensity,
        d3 > 0 ? d3 : -baseDensity,
        getOffsetFromD0(xb, yb)
    );
}

float getDensityMult(int sx, int sy, float xb, float yb, float baseDensity, const TileGrass grassDataArray[PADDED_CHUNK_WIDTH][PADDED_CHUNK_WIDTH], TileGrassID id) {
    const f32 d0 = (f32)grassDataArray[sy][sx].getDensity(id) * DIVIDE_MULT;
    const f32 d1 = (f32)grassDataArray[sy][sx + 1].getDensity(id) * DIVIDE_MULT;
    const f32 d2 = (f32)grassDataArray[sy + 1][sx].getDensity(id) * DIVIDE_MULT;
    const f32 d3 = (f32)grassDataArray[sy + 1][sx + 1].getDensity(id) * DIVIDE_MULT;
    // We use negative of our density if zero, so we get a nice transition instead of a hard edge (iamverysmart)
    return interpolateDensity(
        d0 > 0 ? d0 : -baseDensity,
        d1 > 0 ? d1 : -baseDensity,
        d2 > 0 ? d2 : -baseDensity,
        d3 > 0 ? d3 : -baseDensity,
        getOffsetFromD0(xb, yb)
    );
}

f32 getSpawnChance(int tileX, int tileY, int bladeX, int bladeY) {
    return Random::getCachedRandomfSpecific((bladeX << 3 + bladeY << 4) * 15 + (tileX << 4) - (tileY << 6));
}

f32 getBladeRand(int tileX, int tileY, int bladeX, int bladeY) {
    constexpr f32 BIG_PRIME1 = 7919;
    constexpr f32 BIG_PRIME2 = 7673;
    return Random::getCachedRandomfSpecific(bladeX + BIG_PRIME1 * bladeY - tileX - tileY * BIG_PRIME2);
}

ui8 getRandomRotation(int tileX, int tileY, int bladeX, int bladeY) {
    constexpr f32 BIG_PRIME1 = 7919;
    return (ui8)(Random::getCachedRandomSpecific(bladeX * BIG_PRIME1 - bladeY - (tileX << 4) + (tileY << 5)) & 0xff); // Fast modulus 256
}

f32 getRandomOffset(float a, float b, int tileX, int tileY, int bladeX, int bladeY) {
    constexpr f32 BIG_PRIME1 = 7673;
    constexpr f32 BIG_PRIME2 = 7919;
    return lerp(a, b, Random::getCachedRandomfSpecific(bladeX - BIG_PRIME1 * bladeY - tileX - tileY * BIG_PRIME2));
}

void addGrass(GrassBillboardMeshBuilder& grassMeshBuilder, const TileGrassData& grassData, const NoiseFunction& grassNoiseFunction, const f32v3& relativePos, const f32v3& normal, float rnd, float detail, float bladeWidth, float densityMult, int tileX, int tileY, int bladeX, int bladeY) {
   
    float rsize = lerp(grassData.mHeightVariance.x, grassData.mHeightVariance.y, rnd);
    const f32 grassNoise = -grassNoiseFunction.compute((f64)relativePos.x, (f64)relativePos.y);
    rsize += -grassNoise * 0.4f;
    rsize *= densityMult;
    rsize = glm::max(rsize, 0.15f);

    const f32 zOffset = getRandomOffset(grassData.mZOffsetVariance.x, grassData.mZOffsetVariance.y, tileX, tileY, bladeX, bladeY);
    const ui8 rotation = getRandomRotation(tileX, tileY, bladeX, bladeY);
    grassMeshBuilder.addBladeQuad(
        grassData.mMeshType,
        f32v3(relativePos.x, relativePos.y, relativePos.z + zOffset), // TODO: new height
        f32v2(grassData.mMeshType == TileGrassMeshType::DEFAULT ? bladeWidth : rsize, rsize),
        (ui8)grassData.mId,
        rotation,
        normal
    );
}

void GrassMeshBuilderMethods::createGrassMesh(GrassBillboardMeshBuilder& grassMeshBuilder, const Chunk& chunk, const ui32v2& tilePosStart, ui32 lod, const HeightmapPatchData* heightData)
{
    PROFILE_FUNCTION();
    const TileGrassRepository& grassRepository = Services::ResourceManager::ref().getTileGrassRepository();
    const ui32v2& dims = (ui32v2&)ChunkGrassFlatQuadtree::LOD_DIMS[lod];
    const f32 bladeWidth = GRASS_BLADE_WIDTHS[lod];
    grassMeshBuilder.reserveQuadCount(TileGrassMeshType::DEFAULT, (size_t)dims.x * dims.y * SQ(MAX_GRASS_DETAIL));
    grassMeshBuilder.reserveQuadCount(TileGrassMeshType::PLANE, (size_t)dims.x * dims.y * SQ(MAX_GRASS_DETAIL) / 2);
    grassMeshBuilder.reserveQuadCount(TileGrassMeshType::BILLBOARD, (size_t)dims.x * dims.y * SQ(MAX_GRASS_DETAIL) / 2);
    static_assert(e_count(TileGrassMeshType) == 3);

    TileGrass paddedGrassData[PADDED_CHUNK_WIDTH][PADDED_CHUNK_WIDTH];
    chunk.copyPaddedGrassDataWorkerThread(paddedGrassData);

    // Bounding sphere
    // TODO: This isn't accurate for slopey surfaces! We need a proper AABB
    BoundingSphere boundingSphere;
    // A little algebra ;P
    const f32 halfDims = dims.x * 0.5f;
    boundingSphere.radius = sqrt(2.0f * halfDims * halfDims);
    const f32v2 chunkWorldPos = chunk.getWorldPos();
    boundingSphere.center.x = chunkWorldPos.x + tilePosStart.x + halfDims;
    boundingSphere.center.y = chunkWorldPos.y + tilePosStart.y + halfDims;
    
    IHeightmapGrid& heightmapGrid = chunk.getWorld().getHeightmapGrid();
    const HeightmapPatchID heightmapPatchId = heightmapGrid.getSpatialGrid2D().getIDAtWorldPos(i32v2(chunkWorldPos));
    { // Read lock
        std::shared_lock lock(heightData->mMutex); // TODO: This can be locked for a long time, we need a worker thread copy function

        // Sample bounding sphere from heightmap
        boundingSphere.center.z = heightmapGrid.computeHeightAtChunkOffset(heightData->data, chunk.getChunkID(), f32v2(tilePosStart.x + halfDims, tilePosStart.y + halfDims));
        const TileSpatialGrid& tileSpatialGrid = chunk.getTileContainer()->getTileSpatialGrid();
        // TODO: Optimize redundant math

        constexpr f32 DIVIDE_MULT = 1.0f / 255.0f;

        for (ui32 y = 0; y < dims.y; ++y) {
            const ui32 ty = tilePosStart.y + y;
            const ui32 paddedY = ty + 1;
            for (ui32 x = 0; x < dims.x; ++x) {
                assert(tilePosStart.x + x < CHUNK_WIDTH && tilePosStart.y + y < CHUNK_WIDTH);
                const ui32 tx = tilePosStart.x + x;
                const ui32 paddedX = tx + 1;

                const TileIndex tileIndex = tileSpatialGrid.getBaseTileIndexFromXYOffset(tx, ty);
                const TileGrass& grassVal = paddedGrassData[paddedY][paddedX];

                for (int i = 0; i < MAX_GRASS_TYPES_PER_TILE; ++i) {
                    // TODO: DENSITY
                    const TileGrassID id = grassVal.grassIDs[i];
                    // TODO: This continue will create hard edges. Is that OK?
                    if (id == INVALID_TILE_GRASS_ID) {
                        continue;
                    }

                    const TileGrassData& grassData = grassRepository.getTileGrassData(id);
                    const NoiseFunction& grassNoiseFunction = grassData.mNoiseFunction;
                    const ui32 detail = GRASS_LOD_DETAIL[grassData.mDensity][lod];
                    const f32 baseDensity = grassVal.densities[i] * DIVIDE_MULT;

                    const f32v2 tileWorldOffset = f32v2(tx, ty);

                    // Handle variant UVs
                    constexpr int NUM_GRASS_TYPES = 12;

                    // TODO: Determine edge

                    // Generate blades
                    for (int y2 = 0; y2 < (int)detail; ++y2) {
                        const f32 yb = (f32)y2 / detail;
                        const int yInterpStartOffset = (int)(yb * 2.0f) - 1;
                        for (int x2 = 0; x2 < (int)detail; ++x2) {
                            const f32 xb = (f32)x2 / detail;
                            const int xInterpStartOffset = (int)(xb * 2.0f) - 1;

                            // Interpolate density
                            const int sx = paddedX + xInterpStartOffset;
                            const int sy = paddedY + yInterpStartOffset;
                            const f32 densityMult = getDensityMult(sx, sy, xb, yb, baseDensity, paddedGrassData, id);

                            const f32 spawnChance = getSpawnChance(x, y, x2, y2);
                            if (SQ(spawnChance) <= densityMult) {

                                //const ui8 variantIndex = (ui8)(Random::getCachedRandomSpecific(-x2 * BIG_PRIME + y2 + (tx << 5) - (ty << 4)) % NUM_GRASS_TYPES);
                                const f32 rnd = getBladeRand(x, y, x2, y2);
                                const float xo = (x2 + rnd) / (float)detail;
                                const float yo = (y2 - rnd) / (float)detail;
                                f32v3 relativePos(tileWorldOffset.x + xo, tileWorldOffset.y + yo, 0.0f);
                                f32v3 normal;
                                relativePos.z = heightmapGrid.computeHeightAndNormalAtPoint(heightmapPatchId, heightData->data, chunkWorldPos + f32v2(relativePos), &normal);
                                addGrass(grassMeshBuilder, grassData, grassNoiseFunction, relativePos, normal, rnd, (f32)detail, bladeWidth, densityMult, x, y, x2, y2);
                            }
                        }
                    }
                }
            }
        }
    } // Read lock end
    grassMeshBuilder.setBoundingSphere(boundingSphere);
}

void GrassMeshBuilderMethods::editorCreateGrassMesh(GrassBillboardMeshBuilder& grassMeshBuilder, ui32 widthTiles, const TileGrass grassDataArray[] /* Should be length SQ(widthTiles) */)
{
    PROFILE_FUNCTION();
    const ui32 lod = GRASS_QUADTREE_MAX_LOD - 1;
    const TileGrassRepository& grassRepository = Services::ResourceManager::ref().getTileGrassRepository();
    const f32 bladeWidth = GRASS_BLADE_WIDTHS[lod];
    const ui32 totalTiles = SQ(widthTiles);
    grassMeshBuilder.reserveQuadCount(TileGrassMeshType::DEFAULT, (size_t)totalTiles * SQ(MAX_GRASS_DETAIL));
    grassMeshBuilder.reserveQuadCount(TileGrassMeshType::PLANE, (size_t)totalTiles * SQ(MAX_GRASS_DETAIL) / 2);
    grassMeshBuilder.reserveQuadCount(TileGrassMeshType::BILLBOARD, (size_t)totalTiles * SQ(MAX_GRASS_DETAIL) / 2);
    static_assert(e_count(TileGrassMeshType) == 3);

    // Bounding sphere
    // TODO: This isn't accurate for slopey surfaces! We need a proper AABB
    BoundingSphere boundingSphere;
    // A little algebra ;P
    const f32 halfDims = widthTiles * 0.5f;
    boundingSphere.radius = sqrt(2.0f * halfDims * halfDims);
    boundingSphere.center.x = halfDims;
    boundingSphere.center.y = halfDims;
    boundingSphere.center.z = 0.0f;

    { 
        for (ui32 y = 0; y < widthTiles; ++y) {
            for (ui32 x = 0; x < widthTiles; ++x) {

                const TileIndex tileIndex = y * widthTiles + x;
                const TileGrass& grassVal = grassDataArray[tileIndex];

                for (int i = 0; i < MAX_GRASS_TYPES_PER_TILE; ++i) {
                    // TODO: DENSITY
                    const TileGrassID id = grassVal.grassIDs[i];
                    // TODO: This continue will create hard edges. Is that OK?
                    if (id == INVALID_TILE_GRASS_ID) {
                        continue;
                    }

                    const TileGrassData& grassData = grassRepository.getTileGrassData(id);
                    const NoiseFunction& grassNoiseFunction = grassData.mNoiseFunction;
                    assert(grassData.mDensity <= MAX_GRASS_DETAIL);
                    const ui32 detail = GRASS_LOD_DETAIL[grassData.mDensity][lod];
                    const f32 baseDensity = grassVal.densities[i] * DIVIDE_MULT;

                    const f32v2 tileWorldOffset = f32v2(x, y);

                    // Handle variant UVs
                    constexpr int NUM_GRASS_TYPES = 12;

                    // TODO: Determine edge

                    constexpr auto boundsCheckGetDensity = [](int x, int y, int width, const TileGrass* grassData, TileGrassID id) -> f32 {
                        if (x < 0 || y < 0 || x >= width || y >= width) return 0.0f;
                        return grassData[y * width + x].getDensity(id) * DIVIDE_MULT;
                    };

                    // Generate blades
                    for (int y2 = 0; y2 < (int)detail; ++y2) {
                        const f32 yb = (f32)y2 / detail;
                        const int yInterpStartOffset = (int)(yb * 2.0f) - 1;
                        for (int x2 = 0; x2 < (int)detail; ++x2) {
                            const f32 xb = (f32)x2 / detail;
                            const int xInterpStartOffset = (int)(xb * 2.0f) - 1;

                            // Interpolate density
                            const int sx = x + xInterpStartOffset;
                            const int sy = y + yInterpStartOffset;
                            const f32 densityMult = boundsCheckGetDensityMult(sx, sy, xb, yb, widthTiles, baseDensity, grassDataArray, id);

                            const f32 spawnChance = getSpawnChance(x, y, x2, y2);
                            if (SQ(spawnChance) <= densityMult) {

                                const f32 rnd = getBladeRand(x, y, x2, y2);
                                const float xo = (x2 + rnd) / (float)detail;
                                const float yo = (y2 - rnd) / (float)detail;
                                f32v2 bladePos(tileWorldOffset.x + xo, tileWorldOffset.y + yo);
                                addGrass(grassMeshBuilder, grassData, grassNoiseFunction, f32v3(bladePos.x, bladePos.y, 0.0f), f32v3(0.0f, 0.0f, 1.0f), rnd, detail, bladeWidth, densityMult, x, y, x2, y2);
                            }
                        }
                    }
                }
            }
        }
    } // Read lock end
    grassMeshBuilder.setBoundingSphere(boundingSphere);
}