#include "stdafx.h"
#include "ChunkMesher.h"
#include "rendering/TextureAtlas.h"
#include "services/Services.h"

#include "world/Chunk.h"
#include "world/TileRepository.h"
#include "rendering/QuadMesh.h"
#include "rendering/SpriteData.h"
#include "Random.h"
#include <Vorb/graphics/SamplerState.h>

// For grass noise
#include "generation/WorldGenerationData.h"

constexpr int MAX_CONCURRENT_MESH_TASKS = 10;
constexpr float LAYER_DEPTH_ADD = 0.001f;
constexpr float AMBIENT_OCCLUSION_MULT = 0.7f;
constexpr float NO_AMBIENT_OCCLUSION = 1.0f;

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

enum ExposedNeighborBits {
    EN_BOTTOM_LEFT = 1 << 0,
    EN_BOTTOM = 1 << 1,
    EN_BOTTOM_RIGHT = 1 << 2,
    EN_LEFT = 1 << 3,
    EN_RIGHT = 1 << 4,
    EN_TOP_LEFT = 1 << 5,
    EN_TOP = 1 << 6,
    EN_TOP_RIGHT = 1 << 7
};

const f32v2 CONNECTED_WALL_DIMS = f32v2(6.0f, 5.0f);
const f32v2 VERTICAL_WALL_DIMS = f32v2(1.0f, 3.0f);

inline bool isBitSet(int v, ExposedNeighborBits bit) {
    return v & bit;
}

inline bool areAnyBitsSet(int v, int bits) {
    return v & bits;
}

inline bool areBitsSet(int v, int bits) {
    return (v & bits) == bits;
}

ui8 getCornerIndex(ExposedNeighborBits cornerBit) {
    switch (cornerBit) {
        case EN_BOTTOM_LEFT:
            return 0x1;
        case EN_BOTTOM_RIGHT:
            return 0x2;
        case EN_TOP_LEFT:
            return 0x3;
        case EN_TOP_RIGHT:
            return 0x4;
        default:
            assert(false);
    }
    return 0;
}

bool checkLShape(int i, int lBits, ui32 lIndex, ExposedNeighborBits cornerBit) {
    if (areBitsSet(i, lBits)) {
        sConnectedWallData[i].a = lIndex;
        if (isBitSet(i, cornerBit)) {
            sConnectedWallData[i].b = getCornerIndex(cornerBit);
        }
        return true;
    }
    return false;
}

inline bool checkIShape(int i, ExposedNeighborBits iBit, ui32 iIndex, ExposedNeighborBits cornerBit1, ExposedNeighborBits cornerBit2, ExposedNeighborBits oppositeBit, ui32 oppositeIndex) {
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
        if (areBitsSet(i, EN_LEFT | EN_TOP | EN_RIGHT | EN_BOTTOM)) {
            // No connections
            sConnectedWallData[i].a = 0x11;
            continue;
        }

        // U shapes
        if (areBitsSet(i, EN_LEFT | EN_TOP | EN_RIGHT)) {
            sConnectedWallData[i].a = 0x10;
            continue;
        }
        if (areBitsSet(i, EN_BOTTOM | EN_TOP | EN_RIGHT)) {
            sConnectedWallData[i].a = 0xf;
            continue;
        }
        if (areBitsSet(i, EN_BOTTOM | EN_TOP | EN_LEFT)) {
            sConnectedWallData[i].a = 0xe;
            continue;
        }
        if (areBitsSet(i, EN_LEFT | EN_BOTTOM | EN_RIGHT)) {
            sConnectedWallData[i].a = 0xd;
            continue;
        }

        // L shapes
        if (checkLShape(i, EN_TOP | EN_RIGHT, 0xc, EN_BOTTOM_LEFT)) {
            continue;
        }
        if (checkLShape(i, EN_TOP | EN_LEFT, 0xb, EN_BOTTOM_RIGHT)) {
            continue;
        }
        if (checkLShape(i, EN_BOTTOM | EN_RIGHT, 0xa, EN_TOP_LEFT)) {
            continue;
        }
        if (checkLShape(i, EN_BOTTOM | EN_LEFT, 0x9, EN_TOP_RIGHT)) {
            continue;
        }

        // I shapes
        if (checkIShape(i, EN_TOP, 0x8, EN_BOTTOM_LEFT, EN_BOTTOM_RIGHT, EN_BOTTOM, 0x5)) {
            continue;
        }
        if (checkIShape(i, EN_RIGHT, 0x7, EN_BOTTOM_LEFT, EN_TOP_LEFT, EN_LEFT, 0x6)) {
            continue;
        }
        if (checkIShape(i, EN_LEFT, 0x6, EN_BOTTOM_RIGHT, EN_TOP_RIGHT, EN_RIGHT, 0x7)) {
            continue;
        }
        if (checkIShape(i, EN_BOTTOM, 0x5, EN_TOP_LEFT, EN_TOP_RIGHT, EN_TOP, 0x8)) {
            continue;
        }

        // Finally, corners
        int j = 0;
        if (isBitSet(i, EN_BOTTOM_LEFT)) {
            sConnectedWallData[i].dataArray[j++] = getCornerIndex(EN_BOTTOM_LEFT);
        }
        if (isBitSet(i, EN_BOTTOM_RIGHT)) {
            sConnectedWallData[i].dataArray[j++] = getCornerIndex(EN_BOTTOM_RIGHT);
        }
        if (isBitSet(i, EN_TOP_LEFT)) {
            sConnectedWallData[i].dataArray[j++] = getCornerIndex(EN_TOP_LEFT);
        }
        if (isBitSet(i, EN_TOP_RIGHT)) {
            sConnectedWallData[i].dataArray[j++] = getCornerIndex(EN_TOP_RIGHT);
        }
    }
}

ChunkMesher::ChunkMesher(const TextureAtlas& textureAtlas) :
    mTextureAtlas(textureAtlas)
{
    initConnectedOffsets();
}

ChunkMesher::~ChunkMesher()
{
    // TODO: We will leak any data currently assigned to a worker thread
    for (TileMeshData* data : mFreeTileMeshData) {
        delete data;
    }
}

void uploadLODTexture(ChunkRenderData& renderData, color3* pixelData) {
    if (!renderData.mLODTexture) {
        glGenTextures(1, &renderData.mLODTexture);
    }

    glBindTexture(GL_TEXTURE_2D, renderData.mLODTexture);
    // Compressed
    glTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGB, CHUNK_WIDTH, CHUNK_WIDTH, 0, GL_RGB, GL_UNSIGNED_BYTE, pixelData);

    // Set up tex parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    glBindTexture(GL_TEXTURE_2D, 0);
}

const ExposedNeighborBits EXPOSED_NEIGHBOR_CARDINAL[4] = {
    EN_BOTTOM,
    EN_LEFT,
    EN_RIGHT,
    EN_TOP
};

const ExposedNeighborBits EXPOSED_NEIGHBOR_ADJACENTS[4][2] = {
    { EN_LEFT, EN_RIGHT }, // EN_BOTTOM
    { EN_BOTTOM, EN_TOP }, // EN_LEFT
    { EN_BOTTOM, EN_TOP }, // EN_RIGHT
    { EN_LEFT, EN_RIGHT }, // EN_TOP
};

const NeighborIndex EXPOSED_NEIGHBOR_ADJACENT_INDICES[4][2] = {
    { NeighborIndex::LEFT, NeighborIndex::RIGHT }, // EN_BOTTOM
    { NeighborIndex::BOTTOM, NeighborIndex::TOP }, // EN_LEFT
    { NeighborIndex::BOTTOM, NeighborIndex::TOP }, // EN_RIGHT
    { NeighborIndex::LEFT, NeighborIndex::RIGHT }, // EN_TOP
};

const QuadFacing EXPOSED_NEIGHBOR_QUAD_FACINGS[4] = {
    QuadFacing::FRONT,
    QuadFacing::LEFT,
    QuadFacing::RIGHT,
    QuadFacing::BACK
};

// Prevent rounding errors, 0.0001 is half a pixel
constexpr f32 UV_EPSILON = 0.0001f;
constexpr f32 UV_EPSILON_2 = 2.0f * UV_EPSILON;


inline int getTileHeight(const Tile& neighbor, int layerIndex) {
    const TileID tileId = neighbor.layers[layerIndex];
    if (tileId == TILE_ID_NONE) {
        return neighbor.baseZPosition;
    }
    const TileData& tileData = TileRepository::getTileData(tileId);
    const SpriteData& spriteData = tileData.spriteData;
    // Transparent tiles appear to be 1 tile lower
    int height = neighbor.baseZPosition;
    if (spriteData.flags & SPRITEDATA_FLAG_OPAQUE) {
        ++height;
    }
    return height;
}

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
    const float layerDepth = layerIndex * LAYER_DEPTH_ADD;
    const Tile& tile = chunk.getTileAtNoAssert(tileIndex);
    const TileID tileId = tile.layers[layerIndex];

    const f32v2 tileWorldPos = chunk.getWorldPos() + f32v2(tileIndex.getX(), tileIndex.getY());

    /*Tile neighbors[8];
    chunk.getTileNeighbors(tileIndex, neighbors);

    const int zPosition = tile.baseZPosition + ((spriteData.flags & SPRITEDATA_FLAG_OPAQUE) ? 1 : 0);
    const int bottomHeightDiff = zPosition - getTileHeight(neighbors[(int)NeighborIndex::BOTTOM], layerIndex);
    const int topHeightDiff = zPosition - getTileHeight(neighbors[(int)NeighborIndex::TOP], layerIndex);*/

    const int x = tileIndex.getX();
    const int y = tileIndex.getY();
    // Allow overlap when adjacent tiles are the same
    const float rightXMult = (rightTile.baseZPosition != tile.baseZPosition || tileId != rightTile.layers[layerIndex]) ? 1.0f : 0.0f;
    const float topXMult = (topTile.baseZPosition != tile.baseZPosition || tileId != topTile.layers[layerIndex]) ? 1.0f : 0.0f;
    ui32 rnd = Random::getThreadSafe(x, y);
    // TODO: Allow grass overlap if right and upper neighbors are same tile + height
    for (int i = 0; i < 5; ++i) {
        const float width = vmath::lerp(0.3f, 0.6f, Random::getCachedRandomfSpecific(rnd));
        const float xOffset = Random::getCachedRandomfSpecific(rnd + 1) * (1.0f - width * rightXMult);
        const float yOffset = Random::getCachedRandomfSpecific(rnd + 2) * (1.0f - width * topXMult);
        floraMesh.addCross(
            f32v3(tileWorldPos.x + xOffset, tileWorldPos.y + yOffset, tile.baseZPosition),
            spriteData.atlasPage,
            spriteData.uvs,
            width,
            COLOR_WHITE,
            spriteData.flags & SPRITEDATA_FLAG_RAND_FLIP,
            255u
        );
        rnd += i * 73; // Add random prime
    }
}

void addTileFloraBillboard(
    BillboardMesh& billboardMesh,
    const Chunk& chunk,
    const TileIndex& tileIndex,
    int layerIndex,
    const TileData& tileData,
    const SpriteData& spriteData,
    const Tile& rightTile,
    const Tile& topTile
) {
    const float layerDepth = layerIndex * LAYER_DEPTH_ADD;
    const Tile& tile = chunk.getTileAtNoAssert(tileIndex);
    const TileID tileId = tile.layers[layerIndex];

    const f32v2 tileWorldPos = chunk.getWorldPos() + f32v2(tileIndex.getX(), tileIndex.getY());

    /*Tile neighbors[8];
    chunk.getTileNeighbors(tileIndex, neighbors);

    const int zPosition = tile.baseZPosition + ((spriteData.flags & SPRITEDATA_FLAG_OPAQUE) ? 1 : 0);
    const int bottomHeightDiff = zPosition - getTileHeight(neighbors[(int)NeighborIndex::BOTTOM], layerIndex);
    const int topHeightDiff = zPosition - getTileHeight(neighbors[(int)NeighborIndex::TOP], layerIndex);*/

    const int x = tileIndex.getX();
    const int y = tileIndex.getY();
    // Allow overlap when adjacent tiles are the same
    const float rightXMult = (rightTile.baseZPosition != tile.baseZPosition || tileId != rightTile.layers[layerIndex]) ? 1.0f : 0.0f;
    const float topXMult = (topTile.baseZPosition != tile.baseZPosition || tileId != topTile.layers[layerIndex]) ? 1.0f : 0.0f;
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
            f32 grassNoise = -sWorldGenData.mGrassNoise.compute((f64)tileWorldPos.x + i * 0.2, (f64)tileWorldPos.y + i * 0.2);

            // Handle variant UVs
            ui32 variantIndex = (ui32)((grassNoise + 1.0f) * SQ(variantCount)) % variantCount;
            ui32 variantY = variantIndex / spriteData.variantCount.x;
            ui32 variantX = variantIndex % spriteData.variantCount.x;

            uvs.x += variantX * spriteData.uvs.z;
            uvs.y += variantY * spriteData.uvs.w;
        }

        billboardMesh.addQuad(
            f32v3(tileWorldPos.x + xOffset, tileWorldPos.y + yOffset, tile.baseZPosition),
            spriteData.dimsMeters * width,
            f32v2(0.0f),
            spriteData.atlasPage,
            uvs,
            COLOR_WHITE,
            (spriteData.flags & SPRITEDATA_FLAG_RAND_FLIP),
            255u
        );
        rnd += i * 73; // Add random prime
    }
}

void addBlockConnectedWall(const Chunk& chunk, const TileIndex& tileIndex, int layerIndex, QuadMesh& quadMesh, f32v3 tilePosition, const SpriteData& spriteData) {
    const bool shouldRandFlip = spriteData.flags & SPRITEDATA_FLAG_RAND_FLIP;
    const Tile& tile = chunk.getTileAtNoAssert(tileIndex);
    const TileID tileId = tile.layers[layerIndex];

    const f32v2 xyWorldPos = chunk.getWorldPos() + f32v2(tileIndex.getX(), tileIndex.getY());
    const f32v3 tileWorldPos(xyWorldPos.x, xyWorldPos.y, tile.baseZPosition);

    Tile neighbors[8];
    chunk.getTileNeighbors(tileIndex, neighbors);

    unsigned exposedBits = 0;
    for (int i = 0; i < 8; ++i) {
        const Tile& neighbor = neighbors[i];
        // If neighbor is different tile, or is lower than us, we are exposed to this neighbor
        // TODO: Better occlusion of solid blocks?
        if (neighbor.layers[layerIndex] != tileId || neighbor.baseZPosition < tile.baseZPosition) {
            exposedBits |= (1 << i);
        }
    }
    const ConnectedWallData& data = sConnectedWallData[exposedBits];
    if (data.data == 0) {
        // We are fully surrounded, just render top
        quadMesh.addAxisAlignedQuad(
            tilePosition + BOX_QUAD_FACING_GEOMETRY_OFFSETS[enum_cast(QuadFacing::TOP)],
            spriteData.dimsMeters,
            spriteData.offset,
            QUAD_FACING_AXIS[enum_cast(QuadFacing::TOP)],
            spriteData.atlasPage,
            spriteData.uvs,
            COLOR_WHITE,
            shouldRandFlip
        );
    }
    else {
        // Render top
        // Check if we need to render the base layer first
        if (data.a < 0x10) {
            quadMesh.addAxisAlignedQuad(
                tilePosition + BOX_QUAD_FACING_GEOMETRY_OFFSETS[enum_cast(QuadFacing::TOP)],
                spriteData.dimsMeters,
                spriteData.offset,
                QUAD_FACING_AXIS[enum_cast(QuadFacing::TOP)],
                spriteData.atlasPage,
                spriteData.uvs,
                COLOR_WHITE,
                shouldRandFlip
            );
        }
        // Render up to 4 textures depending on configuration
        f32v3 tilePosRoof = tileWorldPos + BOX_QUAD_FACING_GEOMETRY_OFFSETS[enum_cast(QuadFacing::TOP)];
        for (int i = 0; i < 4; ++i) {
            const ui16 textureIndex = data.dataArray[i];
            if (textureIndex == 0) break;

            const f32v2 offsets = getUvsOffsetsFromConnectedWallIndex(textureIndex);
            f32v4 uvs = spriteData.uvs;
            uvs.x += offsets.x * uvs.z;
            uvs.y += offsets.y * uvs.w;
            tilePosRoof.z += 0.005f;

            quadMesh.addAxisAlignedQuad(
                tilePosRoof,
                spriteData.dimsMeters,
                spriteData.offset,
                QUAD_FACING_AXIS[enum_cast(QuadFacing::TOP)],
                spriteData.atlasPage,
                uvs,
                COLOR_WHITE,
                shouldRandFlip
            );
        }

        // Get height offsets to adjacent tiles
        const int zPosition = tile.baseZPosition + ((spriteData.flags & SPRITEDATA_FLAG_OPAQUE) ? 1 : 0);
        int heightDiffs[4];
        heightDiffs[0] = zPosition - getTileHeight(neighbors[(int)NeighborIndex::BOTTOM], layerIndex);
        heightDiffs[1] = zPosition - getTileHeight(neighbors[(int)NeighborIndex::LEFT], layerIndex);
        heightDiffs[2] = zPosition - getTileHeight(neighbors[(int)NeighborIndex::RIGHT], layerIndex);
        heightDiffs[3] = zPosition - getTileHeight(neighbors[(int)NeighborIndex::TOP], layerIndex);

        for (int c = 0; c < 4; ++c) {
            ExposedNeighborBits cardinal = EXPOSED_NEIGHBOR_CARDINAL[c];
            const ExposedNeighborBits* adjacents = EXPOSED_NEIGHBOR_ADJACENTS[c];
            QuadFacing quadFacing = EXPOSED_NEIGHBOR_QUAD_FACINGS[c];
            // Render exposed cardinal wall if needed
            if (isBitSet(exposedBits, cardinal)) {
                const f32v3 quadPos = tileWorldPos + BOX_QUAD_FACING_GEOMETRY_OFFSETS[enum_cast(quadFacing)];
                // Simple 2 bit LUT that converts cardinals to 1 or 2
                const unsigned sideCheck = ((exposedBits & adjacents[0]) > 0) | (((exposedBits & adjacents[1]) > 0) << 1);
                const ui16 val = sExposedWallLookup[sideCheck];

                const f32v2 offsets = getUvsOffsetsFromConnectedWallIndex(val);
                f32v4 uvs = spriteData.uvs;
                uvs.x += offsets.x * uvs.z;
                uvs.y += offsets.y * uvs.w;
                quadMesh.addAxisAlignedQuad(
                    quadPos,
                    spriteData.dimsMeters,
                    spriteData.offset,
                    QUAD_FACING_AXIS[enum_cast(quadFacing)],
                    spriteData.atlasPage,
                    uvs,
                    COLOR_WHITE,
                    shouldRandFlip
                );

                // See if we need to add additional "tower" quads if we are exposed deeper on the bottom
                for (int i = 1; i < heightDiffs[c]; ++i) {
                    unsigned sideCheck = 0;
                    const NeighborIndex* adjacents = EXPOSED_NEIGHBOR_ADJACENT_INDICES[c];
                    const Tile& leftNeighbor = neighbors[(int)adjacents[0]];
                    const Tile& rightNeighbor = neighbors[(int)adjacents[1]];
                    int adjustedZPosition = tile.baseZPosition - i;
                    if (leftNeighbor.baseZPosition < adjustedZPosition || leftNeighbor.layers[layerIndex] != tileId) {
                        sideCheck |= 1;
                    }
                    if (rightNeighbor.baseZPosition < adjustedZPosition || rightNeighbor.layers[layerIndex] != tileId) {
                        sideCheck |= 2;
                    }
                    // New exposure check for left and right on towers
                    const ui16 val = sExposedWallLookup[sideCheck];
                    if (val == 0) {
                        continue;
                    }

                    const f32v2 offsets = getUvsOffsetsFromConnectedWallIndex(val);
                    f32v4 uvs = spriteData.uvs;
                    uvs.x += offsets.x * uvs.z;
                    // + 1 for the tall wall variants
                    uvs.y += (offsets.y + 1) * uvs.w;
                    // TODO: this resize here...

                    // TODO: Stretched quads?
                    quadMesh.addAxisAlignedQuad(
                        f32v3(quadPos.x, quadPos.y, quadPos.z - i),
                        spriteData.dimsMeters,
                        spriteData.offset,
                        QUAD_FACING_AXIS[enum_cast(quadFacing)],
                        spriteData.atlasPage,
                        uvs,
                        COLOR_WHITE,
                        shouldRandFlip
                    );
                }
            }
        }
    }
}

void addBlockVertical(const Chunk& chunk, const TileIndex& tileIndex, int layerIndex, QuadMesh& quadMesh, f32v3 tilePosition, const SpriteData& spriteData) {
    const bool shouldRandFlip = spriteData.flags & SPRITEDATA_FLAG_RAND_FLIP;
    const Tile& tile = chunk.getTileAtNoAssert(tileIndex);
    const TileID tileId = tile.layers[layerIndex];

    const f32v2 xyWorldPos = chunk.getWorldPos() + f32v2(tileIndex.getX(), tileIndex.getY());
    const f32v3 tileWorldPos(xyWorldPos.x, xyWorldPos.y, tile.baseZPosition);

    Tile neighbors[8];
    chunk.getTileNeighbors(tileIndex, neighbors);

    unsigned exposedBits = 0;
    for (int i = 0; i < 8; ++i) {
        const Tile& neighbor = neighbors[i];
        // If neighbor is different tile, or is lower than us, we are exposed to this neighbor
        // TODO: Better occlusion of solid blocks?
        if (neighbor.layers[layerIndex] != tileId || neighbor.baseZPosition < tile.baseZPosition) {
            exposedBits |= (1 << i);
        }
    }
    const ConnectedWallData& data = sConnectedWallData[exposedBits];
    if (data.data == 0) {
        // We are fully surrounded, just render top
        quadMesh.addAxisAlignedQuad(
            tilePosition + BOX_QUAD_FACING_GEOMETRY_OFFSETS[enum_cast(QuadFacing::TOP)],
            spriteData.dimsMeters,
            spriteData.offset,
            QUAD_FACING_AXIS[enum_cast(QuadFacing::TOP)],
            spriteData.atlasPage,
            spriteData.uvs,
            COLOR_WHITE,
            shouldRandFlip
        );
    }
    else {
        // Render top
        quadMesh.addAxisAlignedQuad(
            tilePosition + BOX_QUAD_FACING_GEOMETRY_OFFSETS[enum_cast(QuadFacing::TOP)],
            spriteData.dimsMeters,
            spriteData.offset,
            QUAD_FACING_AXIS[enum_cast(QuadFacing::TOP)],
            spriteData.atlasPage,
            spriteData.uvs,
            COLOR_WHITE,
            shouldRandFlip
        );

        // Get height offsets to adjacent tiles
        const int zPosition = tile.baseZPosition + ((spriteData.flags & SPRITEDATA_FLAG_OPAQUE) ? 1 : 0);
        int heightDiffs[4];
        heightDiffs[0] = zPosition - getTileHeight(neighbors[(int)NeighborIndex::BOTTOM], layerIndex);
        heightDiffs[1] = zPosition - getTileHeight(neighbors[(int)NeighborIndex::LEFT], layerIndex);
        heightDiffs[2] = zPosition - getTileHeight(neighbors[(int)NeighborIndex::RIGHT], layerIndex);
        heightDiffs[3] = zPosition - getTileHeight(neighbors[(int)NeighborIndex::TOP], layerIndex);

        for (int c = 0; c < 4; ++c) {
            ExposedNeighborBits cardinal = EXPOSED_NEIGHBOR_CARDINAL[c];
            const ExposedNeighborBits* adjacents = EXPOSED_NEIGHBOR_ADJACENTS[c];
            QuadFacing quadFacing = EXPOSED_NEIGHBOR_QUAD_FACINGS[c];
            // Render exposed cardinal wall if needed
            if (isBitSet(exposedBits, cardinal)) {
                const f32v3 quadPos = tileWorldPos + BOX_QUAD_FACING_GEOMETRY_OFFSETS[enum_cast(quadFacing)];
                // Simple 2 bit LUT that converts cardinals to 1 or 2
                const unsigned sideCheck = ((exposedBits & adjacents[0]) > 0) | (((exposedBits & adjacents[1]) > 0) << 1);
                const ui16 val = 2;

                const f32v2 offsets = getUvsOffsetsFromVerticalWallIndex(val);
                f32v4 uvs = spriteData.uvs;
                uvs.x += offsets.x * uvs.z;
                uvs.y += offsets.y * uvs.w;
                quadMesh.addAxisAlignedQuad(
                    quadPos,
                    spriteData.dimsMeters,
                    spriteData.offset,
                    QUAD_FACING_AXIS[enum_cast(quadFacing)],
                    spriteData.atlasPage,
                    uvs,
                    COLOR_WHITE,
                    shouldRandFlip
                );

                // See if we need to add additional "tower" quads if we are exposed deeper on the bottom
                for (int i = 1; i < heightDiffs[c]; ++i) {
                    unsigned sideCheck = 0;
                    const NeighborIndex* adjacents = EXPOSED_NEIGHBOR_ADJACENT_INDICES[c];
                    const Tile& leftNeighbor = neighbors[(int)adjacents[0]];
                    const Tile& rightNeighbor = neighbors[(int)adjacents[1]];
                    int adjustedZPosition = tile.baseZPosition - i;
                    if (leftNeighbor.baseZPosition < adjustedZPosition || leftNeighbor.layers[layerIndex] != tileId) {
                        sideCheck |= 1;
                    }
                    if (rightNeighbor.baseZPosition < adjustedZPosition || rightNeighbor.layers[layerIndex] != tileId) {
                        sideCheck |= 2;
                    }
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
                        QUAD_FACING_AXIS[enum_cast(quadFacing)],
                        spriteData.atlasPage,
                        uvs,
                        COLOR_WHITE,
                        shouldRandFlip
                    );
                }
            }
        }
    }
}

void addBlock(QuadMesh& quadMesh, TileShape shape, f32v3 tilePosition, const SpriteData& spriteData, const TileIndex& tileIndex, const Chunk& chunk, int layerIndex) {
    switch (spriteData.method) {
        case TileTextureMethod::SIMPLE: {
            // TODO: This shouldn't have to be hard coded to floor
            // We do not mesh TileShape::THIN here, it is a billboard
            if (shape == TileShape::BLOCK) {
                quadMesh.addAxisAlignedQuad(
                    tilePosition + BOX_QUAD_FACING_GEOMETRY_OFFSETS[enum_cast(QuadFacing::TOP)],
                    spriteData.dimsMeters,
                    spriteData.offset,
                    QUAD_FACING_AXIS[enum_cast(QuadFacing::TOP)],
                    spriteData.atlasPage,
                    spriteData.uvs,
                    COLOR_WHITE,
                    spriteData.flags & SPRITEDATA_FLAG_RAND_FLIP
                );
            }
            static_assert(enum_cast(TileShape::COUNT) == 2);
            break;
        }
        case TileTextureMethod::CONNECTED_WALL: {
            addBlockConnectedWall(chunk, tileIndex, layerIndex, quadMesh, tilePosition, spriteData);
            break;
        }
        case TileTextureMethod::VERTICAL: {
            addBlockVertical(chunk, tileIndex, layerIndex, quadMesh, tilePosition, spriteData);
            break;
        }
        case TileTextureMethod::WORLD_TILING: {
            int xOff = (ui32)tilePosition.x % 8;
            int yOff = 7 - (ui32)tilePosition.y % 8;
            quadMesh.addAxisAlignedQuad(
                tilePosition + BOX_QUAD_FACING_GEOMETRY_OFFSETS[enum_cast(QuadFacing::TOP)],
                spriteData.dimsMeters,
                spriteData.offset,
                QUAD_FACING_AXIS[enum_cast(QuadFacing::TOP)],
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
    
    TileMeshData* meshData = tryGetFreeTileMeshData();
    if (!meshData) {
        return false;
    }
    
    ++mNumMeshTasksRunning;
    assert(!chunk.mChunkRenderData.mIsBuildingBaseMesh);
    chunk.mChunkRenderData.mIsBuildingBaseMesh = true;

    // TODO: Move somewhere else?
    chunk.mChunkRenderData.mMeshDirty = false;
    chunk.mChunkRenderData.mLODDirty = false;
    chunk.incRef();

    ChunkRenderData& renderData = chunk.mChunkRenderData;
    if (!renderData.mChunkMesh) {
        renderData.mChunkMesh = std::make_unique<QuadMesh>();
        renderData.mBillboardMesh = std::make_unique<BillboardMesh>();
    }
    
    Services::Threadpool::ref().addTask([&chunk, meshData, &renderData](ThreadPoolWorkerData* workerData) {
        const f32v2& chunkPos = chunk.getWorldPos();

        QuadMesh& quadMesh = *renderData.mChunkMesh;
        quadMesh.reserveQuadCount(CHUNK_SIZE * 2); // Most chunks will have less than 2 quads per tile
        BillboardMesh& billboardMesh = *renderData.mBillboardMesh;
        billboardMesh.reserveQuadCount(CHUNK_SIZE); // Most chunks will have less than 1 quad per tile

        color3* lodData = meshData->mLODTexturePixelBuffer;

        for (int y = 0; y < CHUNK_WIDTH; ++y) {
            for (int x = 0; x < CHUNK_WIDTH; ++x) {
                //  TODO: Multiple world layers
                TileIndex index(x, y);
                const Tile& tile = chunk.mTiles[index];
                for (int layerIndex = 0; layerIndex < TILE_LAYER_COUNT; ++layerIndex) {
                    TileID layerTile = tile.layers[layerIndex];
                    if (layerTile == TILE_ID_NONE) {
                        continue;
                    }
                    const TileData& tileData = TileRepository::getTileData(layerTile);
                    const SpriteData& spriteData = tileData.spriteData;

                    if (spriteData.flags & SPRITEDATA_FLAG_RENDER_LOD) {
                        // Set LOD pixel
                        // TODO: expand trees
                        lodData[index] = tileData.spriteData.lodColor;
                    }

                    // Tile mesh
                    // TODO: Baked AO using a gradient texture instead of vertex colors
                    // Flora mesh ONLY
                    if (tileData.shape == TileShape::THIN) {
                        // Billboards
                        if (spriteData.method == TileTextureMethod::FLORA) {
                            f32v3 tilePosition(x + chunkPos.x + 0.5f, y + chunkPos.y + 0.5f, tile.baseZPosition);
                            Tile rightTile = chunk.getRightTileHandle(index).tile;
                            Tile topTile = chunk.getTopTileHandle(index).tile;
                            addTileFloraBillboard(billboardMesh, chunk, index, layerIndex, tileData, spriteData, rightTile, topTile);
                        }
                        else {
                            f32v3 tilePosition(x + chunkPos.x + 0.5f, y + chunkPos.y + 0.5f, tile.baseZPosition);

                            f32v4 uvs = spriteData.uvs;
                            const ui32 variantCount = spriteData.variantCount.x * spriteData.variantCount.y;

                            if (variantCount) {
                                f32 grassNoise = sWorldGenData.mGrassNoise.compute((f64)tilePosition.x, (f64)tilePosition.y);

                                // Handle variant UVs
                                ui32 variantIndex = (ui32)((grassNoise + 1.0f) * SQ(variantCount)) % variantCount;
                                ui32 variantY = variantIndex / spriteData.variantCount.x;
                                ui32 variantX = variantIndex % spriteData.variantCount.x;

                                uvs.x += variantX * spriteData.uvs.z;
                                uvs.y += variantY * spriteData.uvs.w;
                            }

                            billboardMesh.addQuad(tilePosition, spriteData.dimsMeters, f32v2(0.0f), spriteData.atlasPage, uvs, COLOR_WHITE, (spriteData.flags & SPRITEDATA_FLAG_RAND_FLIP), 255u);
                        }
                    }
                    else if (spriteData.method != TileTextureMethod::FLORA) { // CROSS FLORA IS DONE IN SEPARATE PASS
                        // Standard blocks
                        addBlock(quadMesh, tileData.shape, f32v3(x + chunkPos.x, y + chunkPos.y, tile.baseZPosition), spriteData, index, chunk, layerIndex);
                    }
                }
            }
        }
    }, [this, &chunk, meshData]() {

        ChunkRenderData& renderData = chunk.mChunkRenderData;

        renderData.mChunkMesh->finishMesh(MeshDrawMode::DYNAMIC);
        renderData.mBillboardMesh->finishMesh(MeshDrawMode::DYNAMIC);

        // LOD
        uploadLODTexture(renderData, meshData->mLODTexturePixelBuffer);

        // Recycle and flag as free
        mFreeTileMeshData.push_back(meshData);
        chunk.mChunkRenderData.mIsBuildingBaseMesh = false;

        // Update refcount
        --mNumMeshTasksRunning;
        chunk.decRef();
    });
    return true;
}

bool ChunkMesher::createLODTextureAsync(const Chunk& chunk) {

    TileMeshData* meshData = tryGetFreeTileMeshData();
    if (!meshData) {
        return false;
    }

    ++mNumMeshTasksRunning;
    assert(!chunk.mChunkRenderData.mIsBuildingBaseMesh);
    chunk.mChunkRenderData.mIsBuildingBaseMesh = true;
    chunk.mChunkRenderData.mLODDirty = false;
    chunk.incRef();

    // TODO: We have a race condition if the chunk goes out of memory
    Services::Threadpool::ref().addTask([&chunk, meshData](ThreadPoolWorkerData* workerData) {

        color3* currentPixel = meshData->mLODTexturePixelBuffer;
        for (int index = 0; index < CHUNK_SIZE; ++index) {
            //  TODO: More than just ground
            const Tile& tile = chunk.mTiles[index];
            for (int l = TILE_LAYER_COUNT - 1; l >= 0; --l) {
                TileID layerTile = tile.layers[l];
                if (layerTile == TILE_ID_NONE) {
                    continue;
                }
                // First opaque tile
                const TileData& tileData = TileRepository::getTileData(layerTile);
                if (tileData.spriteData.flags & SPRITEDATA_FLAG_RENDER_LOD) {
                    *currentPixel++ = tileData.spriteData.lodColor;
                    break;
                }
            }
        }


    }, [this, &chunk, meshData]() {

        ChunkRenderData& renderData = chunk.mChunkRenderData;

        // LOD
        uploadLODTexture(renderData, meshData->mLODTexturePixelBuffer);

        // Recycle and flag as free
        mFreeTileMeshData.push_back(meshData);
        chunk.mChunkRenderData.mIsBuildingBaseMesh = false;

        // Update refcount
        --mNumMeshTasksRunning;
        chunk.decRef();
    });
    return true;
}

bool ChunkMesher::createHighDetailFloraMeshAsync(const Chunk& chunk) {

    ++mNumMeshTasksRunning;
    assert(!chunk.mChunkRenderData.mIsBuildingHighDetailFloraMesh);
    chunk.mChunkRenderData.mIsBuildingHighDetailFloraMesh = true;

    // TODO: Move somewhere else?
    chunk.mChunkRenderData.mHighDetailFloraMeshDirty = false;
    chunk.incRef();

    ChunkRenderData& renderData = chunk.mChunkRenderData;
    if (!renderData.mHighDetailFloraMesh) {
        renderData.mHighDetailFloraMesh = std::make_unique<QuadMesh>();
    }

    Services::Threadpool::ref().addTask([&chunk, &renderData](ThreadPoolWorkerData* workerData) {
        const f32v2& chunkPos = chunk.getWorldPos();
        QuadMesh& floraMesh = *renderData.mHighDetailFloraMesh;
        // This is usually not enough but might as well try
        // TODO: Reserve based on graphics settings
        floraMesh.reserveQuadCount(CHUNK_SIZE * 3);

        for (int y = 0; y < CHUNK_WIDTH; ++y) {
            for (int x = 0; x < CHUNK_WIDTH; ++x) {
                //  TODO: Multiple world layers
                TileIndex index(x, y);
                const Tile& tile = chunk.mTiles[index];
                // Flora is never ground level
                for (int layerIndex = TILE_LAYER_MID; layerIndex < TILE_LAYER_COUNT; ++layerIndex) {
                    TileID layerTile = tile.layers[layerIndex];
                    if (layerTile == TILE_ID_NONE) {
                        continue;
                    }
                    const TileData& tileData = TileRepository::getTileData(layerTile);
                    const SpriteData& spriteData = tileData.spriteData;
                    // TODO: Baked AO using a gradient texture instead of vertex colors
                    // Flora mesh ONLY
                    // Thin becomes billboard
                    if (spriteData.method == TileTextureMethod::FLORA && tileData.shape != TileShape::THIN) {
                        Tile rightTile = chunk.getRightTileHandle(index).tile;
                        Tile topTile = chunk.getTopTileHandle(index).tile;
                    
                        addTileFlora(floraMesh, chunk, index, layerIndex, tileData, spriteData, rightTile, topTile);
                    }
                }
            }
        }
    }, [this, &chunk]() {

        ChunkRenderData& renderData = chunk.mChunkRenderData;

        renderData.mHighDetailFloraMesh->finishMesh(MeshDrawMode::DYNAMIC);

        // Recycle and flag as free
        chunk.mChunkRenderData.mIsBuildingHighDetailFloraMesh = false;

        // Update refcount
        --mNumMeshTasksRunning;
        chunk.decRef();
    });
    return true;
}

TileMeshData* ChunkMesher::tryGetFreeTileMeshData()
{
    TileMeshData* meshData;
    if (mFreeTileMeshData.size()) {
        meshData = mFreeTileMeshData.back();
        mFreeTileMeshData.pop_back();
    }
    else {
        if (mNumMeshTasksRunning >= MAX_CONCURRENT_MESH_TASKS) {
            return nullptr;
        }
        meshData = new TileMeshData();
    }
    return meshData;
}
