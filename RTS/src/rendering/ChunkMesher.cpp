#include "stdafx.h"
#include "ChunkMesher.h"

#include "world/Chunk.h"
#include "rendering/ChunkMesh.h"

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

f32v4 getUvsFromConnectedWallIndex(int index) {
    const int offsetX = index % (int)CONNECTED_WALL_DIMS.x;
    const int offsetY = index / (int)CONNECTED_WALL_DIMS.x + 1;

    f32v4 uvs;
    uvs.x = offsetX / CONNECTED_WALL_DIMS.x;
    uvs.y = offsetY / CONNECTED_WALL_DIMS.y;
    uvs.z = 1.0f / CONNECTED_WALL_DIMS.x;
    uvs.w = -1.0f / CONNECTED_WALL_DIMS.y;
    return uvs;
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

ChunkMesher::ChunkMesher() {
    initConnectedOffsets();

    ui32 i = 0;
    for (ui32 v = 0; i < MAX_INDICES_PER_CHUNK; v += 4u) {
        mIndices[i++] = v;
        mIndices[i++] = v + 2;
        mIndices[i++] = v + 3;
        mIndices[i++] = v + 3;
        mIndices[i++] = v + 1;
        mIndices[i++] = v;
    }

    std::cout << "Chunk mesher initialized with memory size" << sizeof(ChunkMesher) / 1000 << " MB\n";

}

void addQuad(ChunkVertex* verts, const f32v2& position, const f32v4& uvs, const color4& color, float depth) {
    // Top Left
    ChunkVertex& vtl = verts[0];
    vtl.pos.x = position.x;
    vtl.pos.y = position.y;
    vtl.pos.z = depth;
    vtl.uvs.x = uvs.x;
    vtl.uvs.y = uvs.y;
    vtl.color = color;
    // Top Right
    ChunkVertex& vtr = verts[1];
    vtr.pos.x = position.x + 1;
    vtr.pos.y = position.y;
    vtr.pos.z = depth;
    vtr.uvs.x = uvs.x + uvs.z;
    vtr.uvs.y = uvs.y;
    vtr.color = color;
    // Bottom Left
    ChunkVertex& vbl = verts[2];
    vbl.pos.x = position.x;
    vbl.pos.y = position.y + 1;
    vbl.pos.z = depth;
    vbl.uvs.x = uvs.x;
    vbl.uvs.y = uvs.y + uvs.w;
    vbl.color = color;
    // Bottom Right
    ChunkVertex& vbr = verts[3];
    vbr.pos.x = position.x + 1;
    vbr.pos.y = position.y + 1;
    vbr.pos.z = depth;
    vbr.uvs.x = uvs.x + uvs.z;
    vbr.uvs.y = uvs.y + uvs.w;
    vbr.color = color;
}

void ChunkMesher::createMesh(const Chunk& chunk) {
    ChunkRenderData& renderData = chunk.mChunkRenderData;
    const i32v2& pos = chunk.getChunkPos();
    const i32v2 offset(pos.x * CHUNK_WIDTH, pos.y * CHUNK_WIDTH);

    if (!renderData.mChunkMesh) {
        renderData.mChunkMesh = std::make_unique<ChunkMesh>();
    }
    ChunkMesh& mesh = *renderData.mChunkMesh;

    int vertexCount = 0;
    VGTexture tmp;
    
    PreciseTimer timer;
    Tile neighbors[8];
    for (int y = 0; y < CHUNK_WIDTH; ++y) {
        for (int x = 0; x < CHUNK_WIDTH; ++x) {
            //  TODO: More than just ground
            TileIndex index(x, y);
            const Tile& tile = chunk.mTiles[index];
            const SpriteData& spriteData = TileRepository::getTileData(tile.groundLayer).spriteData;
            tmp = spriteData.texture;
            switch (spriteData.method) {
                case TileTextureMethod::SIMPLE: {
                    
                    addQuad(mVertices + vertexCount, f32v2(x + offset.x, y + offset.y), spriteData.uvs, color4(1.0f, 1.0f, 1.0f), 0.0f);
                    vertexCount += 4;
                    break;
                }
                case TileTextureMethod::CONNECTED_WALL: {
                    assert(false);
                }
            }
        }
    }
    std::cout << " loop took " << timer.stop() << " ms\n";

    PreciseTimer timer2;
    mesh.setData(mVertices, vertexCount, mIndices, tmp);
    std::cout << " setdata took " << timer2.stop() << " ms\n";
    renderData.mBaseDirty = false;
}

void ChunkMesher::updateSpritebatch(const Chunk& chunk) {
    // mutable render data
    ChunkRenderData& renderData = chunk.mChunkRenderData;
    const i32v2& pos = chunk.getChunkPos();
    const i32v2 offset(pos.x * CHUNK_WIDTH, pos.y * CHUNK_WIDTH);

    if (!renderData.mBaseMesh) {
        renderData.mBaseMesh = std::make_unique<vg::SpriteBatch>(true, true);
    }

    PreciseTimer timer;
    Tile neighbors[8];
    renderData.mBaseMesh->begin(CHUNK_WIDTH * CHUNK_WIDTH);
    for (int y = 0; y < CHUNK_WIDTH; ++y) {
        for (int x = 0; x < CHUNK_WIDTH; ++x) {
            //  TODO: More than just ground
            TileIndex index(x, y);
            const Tile& tile = chunk.mTiles[index];
            const SpriteData& spriteData = TileRepository::getTileData(tile.groundLayer).spriteData;
            switch (spriteData.method) {
                case TileTextureMethod::SIMPLE: {
                    renderData.mBaseMesh->draw(spriteData.texture, &spriteData.uvs, f32v2(x + offset.x, y + offset.y), f32v2(spriteData.dims), color4(1.0f, 1.0f, 1.0f), 0.0f);
                    break;
                }
                case TileTextureMethod::CONNECTED_WALL: {
                    chunk.getTileNeighbors(index, neighbors);
                    unsigned connectedBits = 0;
                    for (int i = 0; i < 8; ++i) {
                        if (neighbors[i].groundLayer != tile.groundLayer) {
                            connectedBits |= (1 << i);
                        }
                    }
                    const ConnectedWallData& data = sConnectedWallData[connectedBits];
                    const float verticalOffset = 0.749f;
                    if (data.data == 0) {
                        // TODO: Atlas? Starting offset?
                        f32v4 uvs(0.0f, 0.0f, 1.0f / CONNECTED_WALL_DIMS.x, -1.0f / CONNECTED_WALL_DIMS.y);
                        renderData.mBaseMesh->draw(spriteData.texture, &uvs, f32v2(x + offset.x, y + offset.y + verticalOffset), f32v2(1.0f), color4(1.0f, 1.0f, 1.0f), 1.0f);
                    }
                    else {
                        // Check if we need to render the base layer first
                        if (data.a < 0x10) {
                            f32v4 uvs(0.0f, 0.0f, 1.0f / CONNECTED_WALL_DIMS.x, -1.0f / CONNECTED_WALL_DIMS.y);
                            renderData.mBaseMesh->draw(spriteData.texture, &uvs, f32v2(x + offset.x, y + offset.y + verticalOffset), f32v2(1.0f), color4(1.0f, 1.0f, 1.0f), 0.9f);
                        }
                        // Render up to 4 textures depending on configuration
                        for (int i = 0; i < 4; ++i) {
                            const ui16 val = data.dataArray[i];
                            if (val == 0) break;

                            const f32v4 uvs = getUvsFromConnectedWallIndex(val);
                            renderData.mBaseMesh->draw(spriteData.texture, &uvs, f32v2(x + offset.x, y + offset.y + verticalOffset), f32v2(1.0f), color4(1.0f, 1.0f, 1.0f), 1.0f);
                        }
                        // Render exposed wall bottom if  needed
                        if (isBitSet(connectedBits, BOTTOM)) {
                            // Simple 2 bit LUT
                            const unsigned sideCheck = ((connectedBits & (LEFT | RIGHT)) >> 3);
                            const ui16 val = sExposedWallLookup[sideCheck];

                            const f32v4 uvs = getUvsFromConnectedWallIndex(val);
                            renderData.mBaseMesh->draw(spriteData.texture, &uvs, f32v2(x + offset.x, y + offset.y), f32v2(1.0f), color4(1.0f, 1.0f, 1.0f), 0.0f);
                        }

                    }
                    break;
                }
                default:
                    assert(false);
            }
        }
    }
    std::cout << " p1 " << timer.stop() << " ms\n";
    renderData.mBaseMesh->end();

    std::cout << " p2 " << timer.stop() << " ms\n";
    renderData.mBaseDirty = false;
}
