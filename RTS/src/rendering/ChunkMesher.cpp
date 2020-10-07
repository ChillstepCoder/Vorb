#include "stdafx.h"
#include "ChunkMesher.h"
#include "rendering/TextureAtlas.h"

#include "world/Chunk.h"
#include "rendering/QuadMesh.h"

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
const ui16 sExposedWallLookup[4] = { 0x13, 0x12, 0x14, 0x15 };

enum NeighborBits {
    BOTTOM_LEFT = 1 << 0,
    BOTTOM = 1 << 1,
    BOTTOM_RIGHT = 1 << 2,
    LEFT = 1 << 3,
    RIGHT = 1 << 4,
    TOP_LEFT = 1 << 5,
    TOP = 1 << 6,
    TOP_RIGHT = 1 << 7
};

const f32v2 CONNECTED_WALL_DIMS = f32v2(6.0f, 4.0f);

inline bool isBitSet(int v, NeighborBits bit) {
    return v & bit;
}

inline bool areBitsSet(int v, int bits) {
    return (v & bits) == bits;
}

ui8 getCornerIndex(NeighborBits cornerBit) {
    switch (cornerBit) {
        case BOTTOM_LEFT:
            return 0x1;
        case BOTTOM_RIGHT:
            return 0x2;
        case TOP_LEFT:
            return 0x3;
        case TOP_RIGHT:
            return 0x4;
        default:
            assert(false);
    }
    return 0;
}

bool checkLShape(int i, int lBits, ui32 lIndex, NeighborBits cornerBit) {
    if (areBitsSet(i, lBits)) {
        sConnectedWallData[i].a = lIndex;
        if (isBitSet(i, cornerBit)) {
            sConnectedWallData[i].b = getCornerIndex(cornerBit);
        }
        return true;
    }
    return false;
}

inline bool checkIShape(int i, NeighborBits iBit, ui32 iIndex, NeighborBits cornerBit1, NeighborBits cornerBit2, NeighborBits oppositeBit, ui32 oppositeIndex) {
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

// Hex map
//  0  1  2  3  4  5
//  6  7  8  9  a  b
//  c  d  e  f 10 11
// Cached after running once
void initConnectedOffsets() {
    // 1 bits represent exposed faces
    for (int i = 0; i < 256; ++i) {
        sConnectedWallData[i].data = 0;

        // Standalone
        if (areBitsSet(i, LEFT | TOP | RIGHT | BOTTOM)) {
            // No connections
            sConnectedWallData[i].a = 0x11;
            continue;
        }

        // U shapes
        if (areBitsSet(i, LEFT | TOP | RIGHT)) {
            sConnectedWallData[i].a = 0x10;
            continue;
        }
        if (areBitsSet(i, BOTTOM | TOP | RIGHT)) {
            sConnectedWallData[i].a = 0xf;
            continue;
        }
        if (areBitsSet(i, BOTTOM | TOP | LEFT)) {
            sConnectedWallData[i].a = 0xe;
            continue;
        }
        if (areBitsSet(i, LEFT | BOTTOM | RIGHT)) {
            sConnectedWallData[i].a = 0xd;
            continue;
        }

        // L shapes
        if (checkLShape(i, TOP | RIGHT, 0xc, BOTTOM_LEFT)) {
            continue;
        }
        if (checkLShape(i, TOP | LEFT, 0xb, BOTTOM_RIGHT)) {
            continue;
        }
        if (checkLShape(i, BOTTOM | RIGHT, 0xa, TOP_LEFT)) {
            continue;
        }
        if (checkLShape(i, BOTTOM | LEFT, 0x9, TOP_RIGHT)) {
            continue;
        }

        // I shapes
        if (checkIShape(i, TOP, 0x8, BOTTOM_LEFT, BOTTOM_RIGHT, BOTTOM, 0x5)) {
continue;
        }
        if (checkIShape(i, RIGHT, 0x7, BOTTOM_LEFT, TOP_LEFT, LEFT, 0x6)) {
            continue;
        }
        if (checkIShape(i, LEFT, 0x6, BOTTOM_RIGHT, TOP_RIGHT, RIGHT, 0x7)) {
            continue;
        }
        if (checkIShape(i, BOTTOM, 0x5, TOP_LEFT, TOP_RIGHT, TOP, 0x8)) {
            continue;
        }

        // Finally, corners
        int j = 0;
        if (isBitSet(i, BOTTOM_LEFT)) {
            sConnectedWallData[i].dataArray[j++] = getCornerIndex(BOTTOM_LEFT);
        }
        if (isBitSet(i, BOTTOM_RIGHT)) {
            sConnectedWallData[i].dataArray[j++] = getCornerIndex(BOTTOM_RIGHT);
        }
        if (isBitSet(i, TOP_LEFT)) {
            sConnectedWallData[i].dataArray[j++] = getCornerIndex(TOP_LEFT);
        }
        if (isBitSet(i, TOP_RIGHT)) {
            sConnectedWallData[i].dataArray[j++] = getCornerIndex(TOP_RIGHT);
        }
    }
}

ChunkMesher::ChunkMesher(const TextureAtlas& textureAtlas) :
    mTextureAtlas(textureAtlas)
{
    initConnectedOffsets();
}

void addQuad(BasicVertex* verts, const f32v2& position, const f32v2& dimsMeters, const f32v4& uvs, const color4& color, float extraHeight, ui16 atlasPage) {
    // Center the sprite
    const float xOffset = -(float)((dimsMeters.x - 1) / 2);

    const float bottomDepth = extraHeight;
    float topDepth = extraHeight;
    color4 bottomColor;
    if (dimsMeters.y > 1.0f) {
        topDepth += dimsMeters.y;
        constexpr float SHADOW_MULT = 0.5f;
        bottomColor = color4(ui8(color.r * SHADOW_MULT), ui8(color.r * SHADOW_MULT), ui8(color.r * SHADOW_MULT), color.a);
    }
    else {
        bottomColor = color;
    }

    // Bottom Left
    BasicVertex& vbl = verts[0];
    vbl.pos.x = position.x + xOffset;
    vbl.pos.y = position.y;
    vbl.pos.z = bottomDepth;
    vbl.uvs.x = uvs.x;
    vbl.uvs.y = uvs.y + uvs.w;
    vbl.color = bottomColor;
    vbl.atlasPage = atlasPage;
    // Bottom Right
    BasicVertex& vbr = verts[1];
    vbr.pos.x = position.x + dimsMeters.x + xOffset;
    vbr.pos.y = position.y;
    vbr.pos.z = bottomDepth;
    vbr.uvs.x = uvs.x + uvs.z;
    vbr.uvs.y = uvs.y + uvs.w;
    vbr.color = bottomColor;
    vbr.atlasPage = atlasPage;
    // Top Left
    BasicVertex& vtl = verts[2];
    vtl.pos.x = position.x + xOffset;
    vtl.pos.y = position.y + dimsMeters.y;
    vtl.pos.z = topDepth;
    vtl.uvs.x = uvs.x;
    vtl.uvs.y = uvs.y;
    vtl.color = color;
    vtl.atlasPage = atlasPage;
    // Top Right
    BasicVertex& vtr = verts[3];
    vtr.pos.x = position.x + dimsMeters.x + xOffset;
    vtr.pos.y = position.y + dimsMeters.y;
    vtr.pos.z = topDepth;
    vtr.uvs.x = uvs.x + uvs.z;
    vtr.uvs.y = uvs.y;
    vtr.color = color;
    vtr.atlasPage = atlasPage;
}

void ChunkMesher::createMesh(const Chunk& chunk) {
    ChunkRenderData& renderData = chunk.mChunkRenderData;
    const i32v2& pos = chunk.getChunkPos();
    const i32v2 offset(pos.x * CHUNK_WIDTH, pos.y * CHUNK_WIDTH);

    if (!renderData.mChunkMesh) {
        renderData.mChunkMesh = std::make_unique<QuadMesh>();
    }
    QuadMesh& mesh = *renderData.mChunkMesh;

    int vertexCount = 0;

    PreciseTimer timer;
    Tile neighbors[8];
    for (int y = 0; y < CHUNK_WIDTH; ++y) {
        for (int x = 0; x < CHUNK_WIDTH; ++x) {
            //  TODO: More than just ground
            TileIndex index(x, y);
            const Tile& tile = chunk.mTiles[index];
            for (int l = 0; l < 3; ++l) {
                TileID layerTile = tile.layers[l];
                if (layerTile == TILE_ID_NONE) {
                    continue;
                }
                float layerDepth = l * 0.001f;
                const SpriteData& spriteData = TileRepository::getTileData(layerTile).spriteData;
                switch (spriteData.method) {
                    case TileTextureMethod::SIMPLE: {
                        addQuad(mVertices + vertexCount, f32v2(x + offset.x, y + offset.y), spriteData.dimsMeters, spriteData.uvs, color4(1.0f, 1.0f, 1.0f), 0.0f + layerDepth, spriteData.atlasPage);
                        vertexCount += 4;
                        break;
                    }
                    case TileTextureMethod::CONNECTED_WALL: {
                        chunk.getTileNeighbors(index, neighbors);
                        unsigned connectedBits = 0;
                        for (int i = 0; i < 8; ++i) {
                            if (neighbors[i].layers[l] != layerTile) {
                                connectedBits |= (1 << i);
                            }
                        }
                        const ConnectedWallData& data = sConnectedWallData[connectedBits];
                        const float verticalOffset = 0.749f;
                        if (data.data == 0) {
                            addQuad(mVertices + vertexCount, f32v2(x + offset.x, y + offset.y + verticalOffset), spriteData.dimsMeters, spriteData.uvs, color4(1.0f, 1.0f, 1.0f), 1.0f + layerDepth, spriteData.atlasPage);
                            vertexCount += 4;
                        }
                        else {
                            // Check if we need to render the base layer first
                            if (data.a < 0x10) {
                                addQuad(mVertices + vertexCount, f32v2(x + offset.x, y + offset.y + verticalOffset), spriteData.dimsMeters, spriteData.uvs, color4(1.0f, 1.0f, 1.0f), 0.95f + layerDepth, spriteData.atlasPage);
                                vertexCount += 4;
                            }
                            // Render up to 4 textures depending on configuration
                            for (int i = 0; i < 4; ++i) {
                                const ui16 val = data.dataArray[i];
                                if (val == 0) break;

                                const f32v2 offsets = getUvsOffsetsFromConnectedWallIndex(val);
                                f32v4 uvs = spriteData.uvs;
                                uvs.x += offsets.x * uvs.z;
                                uvs.y += offsets.y * uvs.w;
                                addQuad(mVertices + vertexCount, f32v2(x + offset.x, y + offset.y + verticalOffset), spriteData.dimsMeters, uvs, color4(1.0f, 1.0f, 1.0f), 1.0f + layerDepth, spriteData.atlasPage);
                                vertexCount += 4;
                            }
                            // Render exposed wall bottom if  needed
                            if (isBitSet(connectedBits, BOTTOM)) {
                                // Simple 2 bit LUT
                                const unsigned sideCheck = ((connectedBits & (LEFT | RIGHT)) >> 3);
                                const ui16 val = sExposedWallLookup[sideCheck];

                                const f32v2 offsets = getUvsOffsetsFromConnectedWallIndex(val);
                                f32v4 uvs = spriteData.uvs;
                                uvs.x += offsets.x * uvs.z;
                                uvs.y += offsets.y * uvs.w;
                                addQuad(mVertices + vertexCount, f32v2(x + offset.x, y + offset.y), spriteData.dimsMeters, uvs, color4(1.0f, 1.0f, 1.0f), layerDepth, spriteData.atlasPage);
                                vertexCount += 4;
                            }

                        }
                        break;
                    }
                }
            }
        }
    }
    std::cout << " loop took " << timer.stop() << " ms\n";

    PreciseTimer timer2;
    mesh.setData(mVertices, vertexCount, mTextureAtlas.getAtlasTexture());
    std::cout << " setdata took " << timer2.stop() << " ms\n";
    renderData.mBaseDirty = false;
}
