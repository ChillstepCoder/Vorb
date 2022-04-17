#include "stdafx.h"
#include "ChunkMesher.h"
#include "rendering/TextureAtlas.h"

#include "world/Chunk.h"
#include "world/TileRepository.h"
#include "rendering/QuadMesh.h"
#include "rendering/SpriteData.h"
#include "Random.h"
#include "options/DebugOptions.h"
#include <Vorb/graphics/SamplerState.h>

#include "world/WorldGrid.h"

// For grass noise
#include "generation/WorldGeneration.h"

constexpr float LAYER_DEPTH_ADD = 0.001f;

// TODO: Method(s) file?
struct ConnectedWallData {
    union {
        struct {
            ui8 a;
            ui8 b;
            ui8 c;
            ui8 d;
        };
        ui8 dataArray[4];
        ui32 data;
    };
};

ConnectedWallData sConnectedWallData[256];
const ui16 sExposedWallLookup[4] = { 0x13, 0x12, 0x14, 0x15 }; // These are texture indexes corresponding to the walls

enum ExposedNeighbor8Bits {
    EN8_BOTTOM_LEFT = 1 << 0,
    EN8_BOTTOM = 1 << 1,
    EN8_BOTTOM_RIGHT = 1 << 2,
    EN8_LEFT = 1 << 3,
    EN8_RIGHT = 1 << 4,
    EN8_TOP_LEFT = 1 << 5,
    EN8_TOP = 1 << 6,
    EN8_TOP_RIGHT = 1 << 7
};
enum ExposedNeighbor4Bits {
    EN4_BOTTOM = 1 << 0,
    EN4_LEFT = 1 << 1,
    EN4_RIGHT = 1 << 2,
    EN4_TOP = 1 << 3,
};

const f32v2 CONNECTED_WALL_DIMS = f32v2(6.0f, 5.0f);
const f32v2 VERTICAL_WALL_DIMS = f32v2(1.0f, 3.0f);

inline bool isBitSet(int v, ExposedNeighbor8Bits bit) {
    return v & bit;
}

inline bool isBitSet(int v, ExposedNeighbor4Bits bit) {
    return v & bit;
}

inline bool areAnyBitsSet(int v, int bits) {
    return v & bits;
}

inline bool areBitsSet(int v, int bits) {
    return (v & bits) == bits;
}

ui8 getCornerIndex(ExposedNeighbor8Bits cornerBit) {
    switch (cornerBit) {
        case EN8_BOTTOM_LEFT:
            return 0x1;
        case EN8_BOTTOM_RIGHT:
            return 0x2;
        case EN8_TOP_LEFT:
            return 0x3;
        case EN8_TOP_RIGHT:
            return 0x4;
        default:
            assert(false);
    }
    return 0;
}

bool checkLShape(int i, int lBits, ui32 lIndex, ExposedNeighbor8Bits cornerBit) {
    if (areBitsSet(i, lBits)) {
        sConnectedWallData[i].a = lIndex;
        if (isBitSet(i, cornerBit)) {
            sConnectedWallData[i].b = getCornerIndex(cornerBit);
        }
        return true;
    }
    return false;
}

inline bool checkIShape(int i, ExposedNeighbor8Bits iBit, ui32 iIndex, ExposedNeighbor8Bits cornerBit1, ExposedNeighbor8Bits cornerBit2, ExposedNeighbor8Bits oppositeBit, ui32 oppositeIndex) {
    if (isBitSet(i, iBit)) {
        sConnectedWallData[i].a = iIndex;
        if (isBitSet(i, oppositeBit)) {
            sConnectedWallData[i].b = oppositeIndex;
        }
        else {
            if (isBitSet(i, cornerBit1)) {
                sConnectedWallData[i].b = getCornerIndex(cornerBit1);
                if (isBitSet(i, cornerBit2)) {
                    sConnectedWallData[i].c = getCornerIndex(cornerBit2);
                }
            }
            else if (isBitSet(i, cornerBit2)) {
                sConnectedWallData[i].b = getCornerIndex(cornerBit2);
            }
        }
        return true;
    }
    return false;
}

f32v2 getUvsOffsetsFromConnectedWallIndex(int index) {
    f32v2 rv;
    rv.x = float(index % (int)CONNECTED_WALL_DIMS.x);
    rv.y = float(index / (int)CONNECTED_WALL_DIMS.x);
    return rv;
}

f32v2 getUvsOffsetsFromVerticalWallIndex(int index) {
    f32v2 rv;
    rv.x = float(index % (int)VERTICAL_WALL_DIMS.x);
    rv.y = VERTICAL_WALL_DIMS.y - float(index / (int)VERTICAL_WALL_DIMS.x) - 1;
    return rv;
}

// Hex map
//  0  1  2  3  4  5
//  6  7  8  9  a  b
//  c  d  e  f 10 11
// 12 13 14 15 16 17
// Cached after running once
void initConnectedOffsets() {
    // 1 bits represent exposed faces
    for (int i = 0; i < 256; ++i) {
        sConnectedWallData[i].data = 0;

        // Standalone
        if (areBitsSet(i, EN8_LEFT | EN8_TOP | EN8_RIGHT | EN8_BOTTOM)) {
            // No connections
            sConnectedWallData[i].a = 0x11;
            continue;
        }

        // U shapes
        if (areBitsSet(i, EN8_LEFT | EN8_TOP | EN8_RIGHT)) {
            sConnectedWallData[i].a = 0x10;
            continue;
        }
        if (areBitsSet(i, EN8_BOTTOM | EN8_TOP | EN8_RIGHT)) {
            sConnectedWallData[i].a = 0xf;
            continue;
        }
        if (areBitsSet(i, EN8_BOTTOM | EN8_TOP | EN8_LEFT)) {
            sConnectedWallData[i].a = 0xe;
            continue;
        }
        if (areBitsSet(i, EN8_LEFT | EN8_BOTTOM | EN8_RIGHT)) {
            sConnectedWallData[i].a = 0xd;
            continue;
        }

        // L shapes
        if (checkLShape(i, EN8_TOP | EN8_RIGHT, 0xc, EN8_BOTTOM_LEFT)) {
            continue;
        }
        if (checkLShape(i, EN8_TOP | EN8_LEFT, 0xb, EN8_BOTTOM_RIGHT)) {
            continue;
        }
        if (checkLShape(i, EN8_BOTTOM | EN8_RIGHT, 0xa, EN8_TOP_LEFT)) {
            continue;
        }
        if (checkLShape(i, EN8_BOTTOM | EN8_LEFT, 0x9, EN8_TOP_RIGHT)) {
            continue;
        }

        // I shapes
        if (checkIShape(i, EN8_TOP, 0x8, EN8_BOTTOM_LEFT, EN8_BOTTOM_RIGHT, EN8_BOTTOM, 0x5)) {
            continue;
        }
        if (checkIShape(i, EN8_RIGHT, 0x7, EN8_BOTTOM_LEFT, EN8_TOP_LEFT, EN8_LEFT, 0x6)) {
            continue;
        }
        if (checkIShape(i, EN8_LEFT, 0x6, EN8_BOTTOM_RIGHT, EN8_TOP_RIGHT, EN8_RIGHT, 0x7)) {
            continue;
        }
        if (checkIShape(i, EN8_BOTTOM, 0x5, EN8_TOP_LEFT, EN8_TOP_RIGHT, EN8_TOP, 0x8)) {
            continue;
        }

        // Finally, corners
        int j = 0;
        if (isBitSet(i, EN8_BOTTOM_LEFT)) {
            sConnectedWallData[i].dataArray[j++] = getCornerIndex(EN8_BOTTOM_LEFT);
        }
        if (isBitSet(i, EN8_BOTTOM_RIGHT)) {
            sConnectedWallData[i].dataArray[j++] = getCornerIndex(EN8_BOTTOM_RIGHT);
        }
        if (isBitSet(i, EN8_TOP_LEFT)) {
            sConnectedWallData[i].dataArray[j++] = getCornerIndex(EN8_TOP_LEFT);
        }
        if (isBitSet(i, EN8_TOP_RIGHT)) {
            sConnectedWallData[i].dataArray[j++] = getCornerIndex(EN8_TOP_RIGHT);
        }
    }
}

ChunkMesher::ChunkMesher(const WorldGrid& worldGrid, const TextureAtlas& textureAtlas) :
    mWorldGrid(worldGrid),
    mTextureAtlas(textureAtlas)
{
    initConnectedOffsets();
}

ChunkMesher::~ChunkMesher()
{

}

void ChunkMesher::updateMesh(const Chunk& chunk, const f32v3& cameraPos) {
    UNUSED(cameraPos);
    ChunkRenderData& renderData = chunk.mChunkRenderData;
    if (!renderData.mIsBuildingBaseMesh && renderData.mMeshDirty) {
        createMeshAsync(chunk);
    }

    //if (!renderData.mIsBuildingHighDetailFloraMesh) {
    //    const f32 distanceToCamera2 = glm::length2(cameraPos - chunk.getWorldPosCenter3D());
    //    // TODO: better bias
    //    if (distanceToCamera2 < sDebugOptions.mGrassDistanceSq && renderData.mHighDetailFloraMeshDirty) {
    //        createHighDetailFloraMeshAsync(chunk);
    //    }
    //    else if (distanceToCamera2 >= sDebugOptions.mGrassDistanceSq + 10 && renderData.mGrassMesh) {
    //        renderData.mGrassMesh.reset();
    //        renderData.mHighDetailFloraMeshDirty = true;
    //    }
    //}
}

const ExposedNeighbor8Bits EXPOSED_NEIGHBOR_8_CARDINAL[4] = {
    EN8_BOTTOM,
    EN8_LEFT,
    EN8_RIGHT,
    EN8_TOP
};

const ExposedNeighbor8Bits EXPOSED_NEIGHBOR_8_ADJACENTS[4][2] = {
    { EN8_LEFT, EN8_RIGHT }, // EN8_BOTTOM
    { EN8_BOTTOM, EN8_TOP }, // EN8_LEFT
    { EN8_BOTTOM, EN8_TOP }, // EN8_RIGHT
    { EN8_LEFT, EN8_RIGHT }, // EN8_TOP
};
const ExposedNeighbor4Bits EXPOSED_NEIGHBOR_4_ADJACENTS[4][2] = {
    { EN4_LEFT, EN4_RIGHT }, // EN4_BOTTOM
    { EN4_BOTTOM, EN4_TOP }, // EN4_LEFT
    { EN4_BOTTOM, EN4_TOP }, // EN4_RIGHT
    { EN4_LEFT, EN4_RIGHT }, // EN4_TOP
};

const NeighborIndex8 EXPOSED_NEIGHBOR_8_ADJACENT_INDICES[4][2] = {
    { NeighborIndex8::LEFT, NeighborIndex8::RIGHT }, // EN8_BOTTOM
    { NeighborIndex8::BOTTOM, NeighborIndex8::TOP }, // EN8_LEFT
    { NeighborIndex8::BOTTOM, NeighborIndex8::TOP }, // EN8_RIGHT
    { NeighborIndex8::LEFT, NeighborIndex8::RIGHT }, // EN8_TOP
};

const NeighborIndex4 EXPOSED_NEIGHBOR_4_ADJACENT_INDICES[4][2] = {
    { NeighborIndex4::LEFT, NeighborIndex4::RIGHT }, // EN4_BOTTOM
    { NeighborIndex4::BOTTOM, NeighborIndex4::TOP }, // EN4_LEFT
    { NeighborIndex4::BOTTOM, NeighborIndex4::TOP }, // EN4_RIGHT
    { NeighborIndex4::LEFT, NeighborIndex4::RIGHT }, // EN4_TOP
};

const CubeFacing EXPOSED_NEIGHBOR_QUAD_FACINGS[4] = {
    CubeFacing::FRONT,
    CubeFacing::LEFT,
    CubeFacing::RIGHT,
    CubeFacing::BACK
};

// Prevent rounding errors, 0.0001 is half a pixel
constexpr f32 UV_EPSILON = 0.0001f;
constexpr f32 UV_EPSILON_2 = 2.0f * UV_EPSILON;

void addTileFlora(
    QuadMesh& floraMesh,
    const Chunk& chunk,
    const TileIndex& tileIndex,
    int layerIndex,
    const TileData& tileData,
    const SpriteData& spriteData,
    const Tile& rightTile,
    const Tile& topTile
) {
    assert(false);
    //const float layerDepth = layerIndex * LAYER_DEPTH_ADD;
    //const Tile& tile = chunk.getTileAtNoAssert(tileIndex);
    //const TileID tileId = tile.layers[layerIndex];

    //const f32v2 tileWorldPos = f32v2(tileIndex.getX(), tileIndex.getY());

    ///*Tile neighbors[8];
    //chunk.getTileNeighbors(tileIndex, neighbors);

    //const int zPosition = tile.baseZPosition + ((spriteData.flags & SPRITEDATA_FLAG_OPAQUE) ? 1 : 0);
    //const int bottomHeightDiff = zPosition - getTileHeight(neighbors[(int)NeighborIndex::BOTTOM], layerIndex);
    //const int topHeightDiff = zPosition - getTileHeight(neighbors[(int)NeighborIndex::TOP], layerIndex);*/

    //const int x = tileIndex.getX();
    //const int y = tileIndex.getY();
    //// Allow overlap when adjacent tiles are the same
    //const float rightXMult = (rightTile.baseZPosition != tile.baseZPosition || tileId != rightTile.layers[layerIndex]) ? 1.0f : 0.0f;
    //const float topXMult = (topTile.baseZPosition != tile.baseZPosition || tileId != topTile.layers[layerIndex]) ? 1.0f : 0.0f;
    //ui32 rnd = Random::getThreadSafe(x, y);
    //// TODO: Allow grass overlap if right and upper neighbors are same tile + height
    //for (int i = 0; i < 5; ++i) {
    //    const float width = vmath::lerp(0.3f, 0.6f, Random::getCachedRandomfSpecific(rnd));
    //    const float xOffset = Random::getCachedRandomfSpecific(rnd + 1) * (1.0f - width * rightXMult);
    //    const float yOffset = Random::getCachedRandomfSpecific(rnd + 2) * (1.0f - width * topXMult);
    //    floraMesh.addCross(
    //        f32v3(tileWorldPos.x + xOffset, tileWorldPos.y + yOffset, tile.baseZPosition),
    //        spriteData.atlasPage,
    //        spriteData.uvs,
    //        width,
    //        COLOR_WHITE,
    //        spriteData.flags & SPRITEDATA_FLAG_RAND_FLIP,
    //        255u
    //    );
    //    rnd += i * 73; // Add random prime
    //}
}

void addTileFloraBillboard(
    ChunkBillboardMesh& billboardMesh,
    const Chunk& chunk,
    const TileIndex& tileIndex,
    int layerIndex,
    const TileData& tileData,
    const SpriteData& spriteData,
    const Tile& rightTile,
    const Tile& topTile
) {

    assert(false); // TODO: real grass

    const float layerDepth = layerIndex * LAYER_DEPTH_ADD;
    const Tile& tile = chunk.getTileAtNoAssert(tileIndex);
    const TileID tileId = tile.getLayersThreadSafe(TILE_FLOOR_GROUND)[layerIndex];

    const f32v2 tileWorldPos = f32v2(tileIndex.getX(), tileIndex.getY());

    /*Tile neighbors[8];
    chunk.getTileNeighbors(tileIndex, neighbors);

    const int zPosition = tile.baseZPosition + ((spriteData.flags & SPRITEDATA_FLAG_OPAQUE) ? 1 : 0);
    const int bottomHeightDiff = zPosition - getTileHeight(neighbors[(int)NeighborIndex::BOTTOM], layerIndex);
    const int topHeightDiff = zPosition - getTileHeight(neighbors[(int)NeighborIndex::TOP], layerIndex);*/

    const int x = tileIndex.getX();
    const int y = tileIndex.getY();
    // Allow overlap when adjacent tiles are the same

    const f32 tileBaseZPosition = tile.getBaseZPositionUncompressedThreadSafe();
    const float rightXMult = (rightTile.getBaseZPositionUncompressedThreadSafe() != tileBaseZPosition || tileId != rightTile.getLayersThreadSafe(TILE_FLOOR_GROUND)[layerIndex]) ? 1.0f : 0.0f;
    const float topXMult = (topTile.getBaseZPositionUncompressedThreadSafe() != tileBaseZPosition || tileId != topTile.getLayersThreadSafe(TILE_FLOOR_GROUND)[layerIndex]) ? 1.0f : 0.0f;
    ui32 rnd = Random::getThreadSafe(x, y);
    ui32 batchCount;
    if (spriteData.bunchCount.x == spriteData.bunchCount.y) {
        batchCount = spriteData.bunchCount.x;
    }
    else {
        batchCount = spriteData.bunchCount.x + Random::getCachedRandomSpecific(rnd++) % (spriteData.bunchCount.y - spriteData.bunchCount.x);
    }
    // TODO: Allow grass overlap if right and upper neighbors are same tile + height
    for (int i = 0; i < batchCount; ++i) {
        const float width = vmath::lerp(spriteData.sizeRange.x, spriteData.sizeRange.y, Random::getCachedRandomfSpecific(rnd++));
        const float xOffset = Random::getCachedRandomfSpecific(rnd++) * (1.0f - width * rightXMult);
        const float yOffset = Random::getCachedRandomfSpecific(rnd++) * (1.0f - width * topXMult);

        // TODO: SHARED FUNCTION
        f32v4 uvs = spriteData.uvs;
        const ui32 variantCount = spriteData.variantCount.x * spriteData.variantCount.y;

        if (variantCount) {
            f32 grassNoise = -sWorldGen.mGrassNoise.compute((f64)tileWorldPos.x + i * 0.2, (f64)tileWorldPos.y + i * 0.2);

            // Handle variant UVs
            ui32 variantIndex = (ui32)((grassNoise + 1.0f) * SQ(variantCount)) % variantCount;
            ui32 variantY = variantIndex / spriteData.variantCount.x;
            ui32 variantX = variantIndex % spriteData.variantCount.x;

            uvs.x += variantX * spriteData.uvs.z;
            uvs.y += variantY * spriteData.uvs.w;
        }

        billboardMesh.addQuad(
            f32v3(tileWorldPos.x + xOffset, tileWorldPos.y + yOffset, tileBaseZPosition),
            spriteData.dimsMeters * width,
            f32v2(0.0f),
            spriteData.atlasPage,
            uvs,
            COLOR_WHITE,
            (spriteData.flags & SPRITEDATA_FLAG_RAND_FLIP),
            255u,
            51u // Roughness
        );
        rnd += i * 73; // Add random prime
    }
}

void addBlockConnectedWall(const Chunk& chunk, const TileIndex& tileIndex, int layerIndex, QuadMesh& quadMesh, f32v3 tilePosition, const SpriteData& spriteData) {
    assert(false);
    //const bool shouldRandFlip = spriteData.flags & SPRITEDATA_FLAG_RAND_FLIP;
    //const Tile& tile = chunk.getTileAtNoAssert(tileIndex);
    //const TileID tileId = tile.layers[layerIndex];

    //const f32v2 xyWorldPos = f32v2(tileIndex.getX(), tileIndex.getY());
    //const f32v3 tileWorldPos(xyWorldPos.x, xyWorldPos.y, tile.baseZPosition);

    //Tile neighbors[8];
    //chunk.getTileNeighbors8(tileIndex, neighbors);

    //unsigned exposedBits = 0;
    //for (int i = 0; i < 8; ++i) {
    //    const Tile& neighbor = neighbors[i];
    //    // If neighbor is different tile, or is lower than us, we are exposed to this neighbor
    //    // TODO: Better occlusion of solid blocks?
    //    if (neighbor.layers[layerIndex] != tileId || neighbor.baseZPosition < tile.baseZPosition) {
    //        exposedBits |= (1 << i);
    //    }
    //}
    //const ConnectedWallData& data = sConnectedWallData[exposedBits];
    //if (data.data == 0) {
    //    // We are fully surrounded, just render top
    //    quadMesh.addAxisAlignedQuad(
    //        tilePosition + CUBE_FACING_GEOMETRY_OFFSETS[enum_cast(CubeFacing::TOP)],
    //        spriteData.dimsMeters,
    //        spriteData.offset,
    //        CubeFacing::TOP,
    //        spriteData.atlasPage,
    //        spriteData.uvs,
    //        COLOR_WHITE,
    //        shouldRandFlip
    //    );
    //}
    //else {
    //    // Render top
    //    // Check if we need to render the base layer first
    //    if (data.a < 0x10) {
    //        quadMesh.addAxisAlignedQuad(
    //            tilePosition + CUBE_FACING_GEOMETRY_OFFSETS[enum_cast(CubeFacing::TOP)],
    //            spriteData.dimsMeters,
    //            spriteData.offset,
    //            CubeFacing::TOP,
    //            spriteData.atlasPage,
    //            spriteData.uvs,
    //            COLOR_WHITE,
    //            shouldRandFlip
    //        );
    //    }
    //    // Render up to 4 textures depending on configuration
    //    f32v3 tilePosRoof = tileWorldPos + CUBE_FACING_GEOMETRY_OFFSETS[enum_cast(CubeFacing::TOP)];
    //    for (int i = 0; i < 4; ++i) {
    //        const ui16 textureIndex = data.dataArray[i];
    //        if (textureIndex == 0) break;

    //        const f32v2 offsets = getUvsOffsetsFromConnectedWallIndex(textureIndex);
    //        f32v4 uvs = spriteData.uvs;
    //        uvs.x += offsets.x * uvs.z;
    //        uvs.y += offsets.y * uvs.w;
    //        tilePosRoof.z += 0.005f;

    //        quadMesh.addAxisAlignedQuad(
    //            tilePosRoof,
    //            spriteData.dimsMeters,
    //            spriteData.offset,
    //            CubeFacing::TOP,
    //            spriteData.atlasPage,
    //            uvs,
    //            COLOR_WHITE,
    //            shouldRandFlip
    //        );
    //    }

    //    // Get height offsets to adjacent tiles
    //    const int zPosition = tile.baseZPosition + ((spriteData.flags & SPRITEDATA_FLAG_OPAQUE) ? 1 : 0);
    //    int heightDiffs[4];
    //    heightDiffs[0] = zPosition - 0.0f;//getTileHeight(neighbors[(int)NeighborIndex8::BOTTOM], layerIndex);
    //    heightDiffs[1] = zPosition - 0.0f;//getTileHeight(neighbors[(int)NeighborIndex8::LEFT], layerIndex);
    //    heightDiffs[2] = zPosition - 0.0f;//getTileHeight(neighbors[(int)NeighborIndex8::RIGHT], layerIndex);
    //    heightDiffs[3] = zPosition - 0.0f;//getTileHeight(neighbors[(int)NeighborIndex8::TOP], layerIndex);
    //    assert(false); // NO WORKY

    //    for (int c = 0; c < 4; ++c) {
    //        ExposedNeighbor8Bits cardinal = EXPOSED_NEIGHBOR_8_CARDINAL[c];
    //        const ExposedNeighbor8Bits* adjacents = EXPOSED_NEIGHBOR_8_ADJACENTS[c];
    //        CubeFacing quadFacing = EXPOSED_NEIGHBOR_QUAD_FACINGS[c];
    //        // Render exposed cardinal wall if needed
    //        if (isBitSet(exposedBits, cardinal)) {
    //            const f32v3 quadPos = tileWorldPos + CUBE_FACING_GEOMETRY_OFFSETS[enum_cast(quadFacing)];
    //            // Simple 2 bit LUT that converts cardinals to 1 or 2
    //            const unsigned sideCheck = ((exposedBits & adjacents[0]) > 0) | (((exposedBits & adjacents[1]) > 0) << 1);
    //            const ui16 val = sExposedWallLookup[sideCheck];

    //            const f32v2 offsets = getUvsOffsetsFromConnectedWallIndex(val);
    //            f32v4 uvs = spriteData.uvs;
    //            uvs.x += offsets.x * uvs.z;
    //            uvs.y += offsets.y * uvs.w;
    //            quadMesh.addAxisAlignedQuad(
    //                quadPos,
    //                spriteData.dimsMeters,
    //                spriteData.offset,
    //                quadFacing,
    //                spriteData.atlasPage,
    //                uvs,
    //                COLOR_WHITE,
    //                shouldRandFlip
    //            );

    //            // See if we need to add additional "tower" quads if we are exposed deeper on the bottom
    //            for (int i = 1; i < heightDiffs[c]; ++i) {
    //                unsigned sideCheck = 0;
    //                const NeighborIndex8* adjacents = EXPOSED_NEIGHBOR_8_ADJACENT_INDICES[c];
    //                const Tile& leftNeighbor = neighbors[(int)adjacents[0]];
    //                const Tile& rightNeighbor = neighbors[(int)adjacents[1]];
    //                int adjustedZPosition = tile.baseZPosition - i;
    //                if (leftNeighbor.baseZPosition < adjustedZPosition || leftNeighbor.layers[layerIndex] != tileId) {
    //                    sideCheck |= 1;
    //                }
    //                if (rightNeighbor.baseZPosition < adjustedZPosition || rightNeighbor.layers[layerIndex] != tileId) {
    //                    sideCheck |= 2;
    //                }
    //                // New exposure check for left and right on towers
    //                const ui16 val = sExposedWallLookup[sideCheck];
    //                if (val == 0) {
    //                    continue;
    //                }

    //                const f32v2 offsets = getUvsOffsetsFromConnectedWallIndex(val);
    //                f32v4 uvs = spriteData.uvs;
    //                uvs.x += offsets.x * uvs.z;
    //                // + 1 for the tall wall variants
    //                uvs.y += (offsets.y + 1) * uvs.w;
    //                // TODO: this resize here...

    //                // TODO: Stretched quads?
    //                quadMesh.addAxisAlignedQuad(
    //                    f32v3(quadPos.x, quadPos.y, quadPos.z - i),
    //                    spriteData.dimsMeters,
    //                    spriteData.offset,
    //                    quadFacing,
    //                    spriteData.atlasPage,
    //                    uvs,
    //                    COLOR_WHITE,
    //                    shouldRandFlip
    //                );
    //            }
    //        }
    //    }
    //}
}

void ChunkMesher::addBlockVertical(const Chunk& chunk, const TileIndex& tileIndex, int layerIndex, QuadMesh& quadMesh, f32v3 tilePosition, const HeightmapPatchData* heightData, const TileData& tileData) {
    // Currently only supported for ground layer
    assert(tileData.layer == TILE_LAYER_GROUND);

    const SpriteData& spriteData = tileData.spriteData;
    const bool shouldRandFlip = spriteData.flags & SPRITEDATA_FLAG_RAND_FLIP;
    const Tile& tile = chunk.getTileAtNoAssert(tileIndex);
    const TileID tileId = tile.getLayersThreadSafe(TILE_FLOOR_GROUND)[layerIndex];

    const ui32v2 xyTilePos(tileIndex.getX(), tileIndex.getY());
    const f32 baseZPosition = tile.getBaseZPositionUncompressedThreadSafe();
    const f32v3 tilePos(xyTilePos.x, xyTilePos.y, baseZPosition);

    TileHandle neighbors[4];
    chunk.getTileNeighbors4(tileIndex, neighbors);

    // Get height offsets to adjacent tiles
    //const f32 zPosition = tile.baseZPosition; // Dont check terrain here, assume above // TODO: make sure this is right
    f32 heightDiffs[4];
    heightDiffs[(int)NeighborIndex4::BOTTOM] = baseZPosition - getTileHeight(neighbors[(int)NeighborIndex4::BOTTOM]);
    heightDiffs[(int)NeighborIndex4::LEFT]   = baseZPosition - getTileHeight(neighbors[(int)NeighborIndex4::LEFT]);
    heightDiffs[(int)NeighborIndex4::RIGHT]  = baseZPosition - getTileHeight(neighbors[(int)NeighborIndex4::RIGHT]);
    heightDiffs[(int)NeighborIndex4::TOP]    = baseZPosition - getTileHeight(neighbors[(int)NeighborIndex4::TOP]);

    // Render top
    quadMesh.addAxisAlignedQuad(
        tilePosition + CUBE_FACING_GEOMETRY_OFFSETS[e_cast(CubeFacing::TOP)],
        spriteData.dimsMeters,
        spriteData.offset,
        CubeFacing::TOP,
        spriteData.atlasPage,
        spriteData.uvs,
        COLOR_WHITE,
        shouldRandFlip
    );

    // Render sides
    for (int c = 0; c < 4; ++c) {
        // Render exposed cardinal wall if needed
        if (heightDiffs[c] > 0.0f) {
            CubeFacing quadFacing = EXPOSED_NEIGHBOR_QUAD_FACINGS[c];
            const f32v3 quadPos = tilePos + CUBE_FACING_GEOMETRY_OFFSETS[e_cast(quadFacing)];
            const f32v2 offsets = getUvsOffsetsFromVerticalWallIndex(2);
            f32v4 uvs = spriteData.uvs;
            uvs.x += offsets.x * uvs.z;
            uvs.y += offsets.y * uvs.w;
            quadMesh.addAxisAlignedQuad(
                quadPos,
                spriteData.dimsMeters,
                spriteData.offset,
                quadFacing,
                spriteData.atlasPage,
                uvs,
                COLOR_WHITE,
                shouldRandFlip
            );

            // See if we need to add additional "tower" quads if we are exposed deeper on the bottom
            for (int i = 1; i < heightDiffs[c]; ++i) {
                unsigned sideCheck = 0;
                // New exposure check for left and right on towers
                const ui16 val = glm::max(2 - i, 0);

                const f32v2 offsets = getUvsOffsetsFromVerticalWallIndex(val);
                f32v4 uvs = spriteData.uvs;
                uvs.x += offsets.x * uvs.z;
                // + 1 for the tall wall variants
                uvs.y += offsets.y * uvs.w;
                // TODO: this resize here...

                // TODO: Stretched quads?
                quadMesh.addAxisAlignedQuad(
                    f32v3(quadPos.x, quadPos.y, quadPos.z - i),
                    spriteData.dimsMeters,
                    spriteData.offset,
                    quadFacing,
                    spriteData.atlasPage,
                    uvs,
                    COLOR_WHITE,
                    shouldRandFlip
                );
            }
        }
    }
}

void ChunkMesher::addFloor(QuadMesh& quadMesh, f32v3 tilePosition, const HeightmapPatchData* heightData, const TileData& tileData, const TileIndex& tileIndex, const Chunk& chunk, int layerIndex) {
    const SpriteData& spriteData = tileData.spriteData;

    f32 corners[4];
    mWorldGrid.computeTileCorners(heightData->data, tileIndex, corners);

    switch (spriteData.method) {
        case TileTextureMethod::SIMPLE: {
            quadMesh.addTerrainAlignedQuad(
                tilePosition + CUBE_FACING_GEOMETRY_OFFSETS[e_cast(CubeFacing::TOP)],
                corners,
                spriteData.atlasPage,
                spriteData.uvs,
                COLOR_WHITE,
                mWorldGrid.areTrianglesFlippedAtTile(tileIndex),
                spriteData.flags & SPRITEDATA_FLAG_RAND_FLIP
            );
            break;
        default:
            assert(false); // Unsupported
        }
    }
}

void ChunkMesher::addBlock(QuadMesh& quadMesh, f32v3 tilePosition, const HeightmapPatchData* heightData, const TileData& tileData, const TileIndex& tileIndex, const Chunk& chunk, int layerIndex) {

    const SpriteData& spriteData = tileData.spriteData;
    switch (spriteData.method) {
        case TileTextureMethod::SIMPLE: {
            quadMesh.addAxisAlignedQuad(
                tilePosition + CUBE_FACING_GEOMETRY_OFFSETS[e_cast(CubeFacing::TOP)],
                spriteData.dimsMeters,
                spriteData.offset,
                CubeFacing::TOP,
                spriteData.atlasPage,
                spriteData.uvs,
                COLOR_WHITE,
                spriteData.flags & SPRITEDATA_FLAG_RAND_FLIP
            );
            break;
        }
        case TileTextureMethod::CONNECTED_WALL: {
            // todo: FIX
            addBlockConnectedWall(chunk, tileIndex, layerIndex, quadMesh, tilePosition, spriteData);
            break;
        }
        case TileTextureMethod::VERTICAL: {
            addBlockVertical(chunk, tileIndex, layerIndex, quadMesh, tilePosition, heightData, tileData);
            break;
        }
        case TileTextureMethod::WORLD_TILING: {
            // todo: FIX
            int xOff = (ui32)tilePosition.x % 8;
            int yOff = 7 - (ui32)tilePosition.y % 8;
            quadMesh.addAxisAlignedQuad(
                tilePosition + CUBE_FACING_GEOMETRY_OFFSETS[e_cast(CubeFacing::TOP)],
                spriteData.dimsMeters,
                spriteData.offset,
                CubeFacing::TOP,
                spriteData.atlasPage,
                spriteData.uvs + f32v4(xOff / 8.0f, yOff / 8.0f, 0.0f, 0.0f),
                COLOR_WHITE,
                spriteData.flags & SPRITEDATA_FLAG_RAND_FLIP
            );
            break;
        }
        case TileTextureMethod::FLORA: {
            // We do not mesh flora in this pass
            break;
        }
    }
    static_assert((int)TileTextureMethod::COUNT == 6, "Implement geo generation for new method");
}


bool ChunkMesher::createMeshAsync(const Chunk& chunk) {
    
    assert(!chunk.mChunkRenderData.mIsBuildingBaseMesh);
    chunk.mChunkRenderData.mIsBuildingBaseMesh = true;

    // TODO: Move somewhere else?
    chunk.mChunkRenderData.mMeshDirty = false;
    chunk.incReadLockAndRefCountNeighbors4AndSelf();

    ChunkRenderData& renderData = chunk.mChunkRenderData;
    if (!renderData.mChunkMesh) {
        renderData.mChunkMesh = std::make_unique<QuadMesh>();
        renderData.mBillboardMesh = std::make_unique<ChunkBillboardMesh>();
    }
    renderData.mBillboardMesh->beginMesh();

    const HeightmapPatchData* heightData = mWorldGrid.getHeightDataAt(chunk.getChunkID());
    
    Services::Threadpool::ref().addTask([this, &chunk, &renderData, heightData](ThreadPoolWorkerData*) {

        QuadMesh& quadMesh = *renderData.mChunkMesh;
        quadMesh.reserveQuadCount(CHUNK_SIZE); // Most chunks will have less than 1 quad per tile
        ChunkBillboardMesh& billboardMesh = *renderData.mBillboardMesh;
        billboardMesh.reserveQuadCount(CHUNK_SIZE); // Most chunks will have less than 1 quad per tile

        for (int y = 0; y < CHUNK_WIDTH; ++y) {
            for (int x = 0; x < CHUNK_WIDTH; ++x) {
                //  TODO: Multiple world layers
                TileIndex index(x, y);
                const Tile& tile = chunk.mTiles[index];
                const f32 baseZPosition = tile.getBaseZPositionUncompressedThreadSafe();
                for (int layerIndex = 0; layerIndex < TILE_LAYER_COUNT; ++layerIndex) {
                    TileID layerTile = tile.getLayersThreadSafe(TILE_FLOOR_GROUND)[layerIndex];
                    if (layerTile == TILE_ID_NONE) {
                        continue;
                    }

                    const TileData& tileData = TileRepository::getTileData(layerTile);
                    const SpriteData& spriteData = tileData.spriteData;

                    // Tile mesh
                    // Flora mesh ONLY
                    if (tileData.shape == TileShape::THIN) {
                        // Billboards
                        if (spriteData.method == TileTextureMethod::FLORA) {
                            /*f32v3 tilePosition(x + 0.5f, y + 0.5f, tile.baseZPosition);
                            Tile rightTile = chunk.getRightTileHandle(index).tile;
                            Tile topTile = chunk.getTopTileHandle(index).tile;
                            addTileFloraBillboard(billboardMesh, chunk, index, layerIndex, tileData, spriteData, rightTile, topTile);*/
                            continue;
                        }
                        else {
                            f32 zPosition = glm::max(baseZPosition, mWorldGrid.computeCenterHeightAtTile(chunk.getChunkID(), index));
                            f32v3 tilePosition(x + 0.5f, y + 0.5f, zPosition);

                            f32v4 uvs = spriteData.uvs;
                            const ui32 variantCount = spriteData.variantCount.x * spriteData.variantCount.y;

                            if (variantCount) {
                                f32 grassNoise = sWorldGen.mGrassNoise.compute((f64)tilePosition.x, (f64)tilePosition.y);

                                // Handle variant UVs
                                ui32 variantIndex = (ui32)((grassNoise + 1.0f) * SQ(variantCount)) % variantCount;
                                ui32 variantY = variantIndex / spriteData.variantCount.x;
                                ui32 variantX = variantIndex % spriteData.variantCount.x;

                                uvs.x += variantX * spriteData.uvs.z;
                                uvs.y += variantY * spriteData.uvs.w;
                            }

                            billboardMesh.addQuad(tilePosition, spriteData.dimsMeters, f32v2(0.0f), spriteData.atlasPage, uvs, COLOR_WHITE, (spriteData.flags & SPRITEDATA_FLAG_RAND_FLIP), 255u, 0u);
                        }
                    }
                    else if (tileData.shape == TileShape::BLOCK) {
                        // Standard blocks
                        addBlock(quadMesh, f32v3(x, y, baseZPosition), heightData, tileData, index, chunk, layerIndex);
                    }
                    else if (tileData.shape == TileShape::FLOOR) {
                       // Standard blocks
                        addFloor(quadMesh, f32v3(x, y, baseZPosition), heightData, tileData, index, chunk, layerIndex);
                    }
                }
            }
        }

        // No longer need read access
        chunk.decReadLockNeighbors4();
        chunk.decReadLock();
        chunk.decRefNeighbors4();
    }, [this, &chunk]() {

        ChunkRenderData& renderData = chunk.mChunkRenderData;

        renderData.mChunkMesh->finishMesh(MeshDrawMode::STATIC);
        renderData.mBillboardMesh->finishMesh(MeshDrawMode::STATIC);

        // Recycle and flag as free
        chunk.mChunkRenderData.mIsBuildingBaseMesh = false;

        // No longer need to exist
        chunk.decRef();
    });
    return true;
}

f32 ChunkMesher::getTileHeight(const Tile& neighbor, const f32* heightData, TileIndex tileIndex) {
    f32 height = 0.0f;
    const TileID tileId = neighbor.getLayersThreadSafe(TILE_FLOOR_GROUND)[TILE_LAYER_GROUND];
    if (tileId != TILE_ID_NONE) {
        const TileData& tileData = TileRepository::getTileData(tileId);
        const SpriteData& spriteData = tileData.spriteData;
        // Transparent tiles do not count
        if (!(spriteData.flags & SPRITEDATA_FLAG_TRANSPARENT)) {
            height = neighbor.getBaseZPositionUncompressedThreadSafe();
        }
    }
    return glm::max(height, mWorldGrid.computeMinHeightAtTile(heightData, tileIndex));
}

f32 ChunkMesher::getTileHeight(const TileHandle& neighbor) {
    const Tile& tile = *neighbor.tile;
    f32 height = 0.0f;
    const TileID tileId = tile.getLayersThreadSafe(TILE_FLOOR_GROUND)[TILE_LAYER_GROUND];
    if (tileId != TILE_ID_NONE) {
        const TileData& tileData = TileRepository::getTileData(tileId);
        const SpriteData& spriteData = tileData.spriteData;
        // Transparent tiles appear to be 1 tile lower
        if (!(spriteData.flags & SPRITEDATA_FLAG_TRANSPARENT)) {
            height = tile.getBaseZPositionUncompressedThreadSafe();
        }
    }
    return glm::max(height, mWorldGrid.computeMinHeightAtTile(neighbor.chunk->getChunkID(), neighbor.index));
}
