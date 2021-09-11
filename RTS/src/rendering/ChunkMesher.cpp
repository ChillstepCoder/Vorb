#include "stdafx.h"
#include "ChunkMesher.h"
#include "rendering/TextureAtlas.h"
#include "services/Services.h"

#include "world/Chunk.h"
#include "rendering/QuadMesh.h"
#include "Random.h"
#include <Vorb/graphics/SamplerState.h>

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

// TODO: Do we need bottom?
enum class QuadFacing {
    LEFT,
    FRONT,
    RIGHT,
    BACK,
    TOP,
    BOTTOM,
    COUNT
};

const i32v2 QUAD_FACING_AXIS[enum_cast(QuadFacing::COUNT)] = {
    i32v2(AXIS_Y, AXIS_Z), // LEFT
    i32v2(AXIS_X, AXIS_Z),  // FRONT
    i32v2(AXIS_Y, AXIS_Z),  // RIGHT
    i32v2(AXIS_X, AXIS_Z), // BACK
    i32v2(AXIS_X, AXIS_Y),  // TOP
    i32v2(AXIS_X, AXIS_Y)   // BOTTOM
};

const i32v3 QUAD_FACING_ADJACENT_OFFSETS[enum_cast(QuadFacing::COUNT)] = {
    i32v3(-1, 0, 0), // LEFT
    i32v3(0, -1, 0), // FRONT
    i32v3(1, 0, 0), // RIGHT
    i32v3(0, 1, 0), // BACK
    i32v3(0, 0, 1),  // TOP
    i32v3(0, 0, -1)  // BOTTOM
};

const f32v3 BOX_QUAD_FACING_GEOMETRY_OFFSETS[enum_cast(QuadFacing::COUNT)] = {
    f32v3(0, 0, -1.0), // LEFT
    f32v3(0, 0, -1.0), // FRONT
    f32v3(1.0f, 0, -1.0), // RIGHT
    f32v3(0, 1.0f, -1.0), // BACK
    f32v3(0, 0, 0.0f),  // TOP
    f32v3(0, 0, -1.0) // BOTTOM
};

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

void addQuad(std::vector<TileVertex>& vertexData, f32v3 tilePosition, QuadFacing facing, const SpriteData& spriteData, const f32v4& uvs) {
    static constexpr float EPSILON = 0.005f;

    vertexData.resize(vertexData.size() + 4);
    TileVertex* verts = &vertexData.back() - 3;

    // Center the sprite
    // TODO: This shouldnt be hard coded to xy
    const f32v2 offset(-(float)((spriteData.dimsMeters.x - 1) / 2) + spriteData.offset.x, spriteData.offset.y);
    tilePosition.x += offset.x;
    tilePosition.y += offset.y;

    f32v4 adjustedUvs;
    if ((spriteData.flags & SPRITEDATA_FLAG_RAND_FLIP) && Random::getThreadSafef(tilePosition.x, tilePosition.y) > 0.5f) {
        // Flip horizontal
        adjustedUvs.x = uvs.x + uvs.z - UV_EPSILON;
        adjustedUvs.y = uvs.y + UV_EPSILON;
        adjustedUvs.z = -uvs.z + UV_EPSILON_2;
        adjustedUvs.w = uvs.w - UV_EPSILON_2;
    }
    else {
        adjustedUvs.x = uvs.x + UV_EPSILON;
        adjustedUvs.y = uvs.y + UV_EPSILON;
        adjustedUvs.z = uvs.z - UV_EPSILON_2;
        adjustedUvs.w = uvs.w - UV_EPSILON_2;
    }

    color4 topColor = color4((ui8)255u, (ui8)255u, (ui8)255u);
    color4 bottomColor = topColor;
    const i32v2& axis = QUAD_FACING_AXIS[enum_cast(facing)];

    { // Bottom Left
        TileVertex& vbl = verts[0];
        vbl.pos = tilePosition;
        vbl.uvs.x = adjustedUvs.x;
        vbl.uvs.y = adjustedUvs.y + adjustedUvs.w;
        vbl.color = bottomColor;
        vbl.atlasPage = spriteData.atlasPage;
    }
    { // Bottom Right
        TileVertex& vbr = verts[1];
        vbr.pos = tilePosition;
        vbr.uvs.x = adjustedUvs.x + adjustedUvs.z;
        vbr.uvs.y = adjustedUvs.y + adjustedUvs.w;
        vbr.color = bottomColor;
        vbr.atlasPage = spriteData.atlasPage;
        vbr.pos[axis.x] += spriteData.dimsMeters.x + EPSILON;
    }

    { // Top Left
        TileVertex& vtl = verts[2];
        vtl.pos = tilePosition;
        vtl.uvs.x = adjustedUvs.x;
        vtl.uvs.y = adjustedUvs.y;
        vtl.color = topColor;
        vtl.atlasPage = spriteData.atlasPage;
        vtl.pos[axis.y] += spriteData.dimsMeters.y + EPSILON;
    }
    { // Top Right
        TileVertex& vtr = verts[3];
        vtr.pos = tilePosition;
        vtr.uvs.x = adjustedUvs.x + adjustedUvs.z;
        vtr.uvs.y = adjustedUvs.y;
        vtr.color = topColor;
        vtr.atlasPage = spriteData.atlasPage;
        vtr.pos[axis.x] += spriteData.dimsMeters.x + EPSILON;
        vtr.pos[axis.y] += spriteData.dimsMeters.y + EPSILON;
    }
}

void addQuad(std::vector<BillboardVertex>& vertexData, f32v3 tilePosition, const SpriteData& spriteData, const f32v4& uvs) {
    static constexpr float EPSILON = 0.005f;

    vertexData.resize(vertexData.size() + 4);
    BillboardVertex* verts = &vertexData.back() - 3;

    //// Center the sprite
    //// TODO: This shouldnt be hard coded to xy
    //const f32v2 offset(-(float)((spriteData.dimsMeters.x - 1) / 2) + spriteData.offset.x, spriteData.offset.y);
    tilePosition.x += 0.5f;
    tilePosition.y += 0.5f;

    f32v4 adjustedUvs;
    if ((spriteData.flags & SPRITEDATA_FLAG_RAND_FLIP) && Random::getThreadSafef(tilePosition.x, tilePosition.y) > 0.5f) {
        // Flip horizontal
        adjustedUvs.x = uvs.x + uvs.z - UV_EPSILON;
        adjustedUvs.y = uvs.y + UV_EPSILON;
        adjustedUvs.z = -uvs.z + UV_EPSILON_2;
        adjustedUvs.w = uvs.w - UV_EPSILON_2;
    }
    else {
        adjustedUvs.x = uvs.x + UV_EPSILON;
        adjustedUvs.y = uvs.y + UV_EPSILON;
        adjustedUvs.z = uvs.z - UV_EPSILON_2;
        adjustedUvs.w = uvs.w - UV_EPSILON_2;
    }

    color4 topColor = color4((ui8)255u, (ui8)255u, (ui8)255u);
    color4 bottomColor = topColor;
    const f32 halfX = spriteData.dimsMeters.x * 0.5f;

    { // Bottom Left
        BillboardVertex& vbl = verts[0];
        vbl.rootPos.x = tilePosition.x;
        vbl.rootPos.y = tilePosition.y;
        vbl.rootPos.z = tilePosition.z;
        vbl.uvs.x = adjustedUvs.x;
        vbl.uvs.y = adjustedUvs.y + adjustedUvs.w;
        vbl.color = bottomColor;
        vbl.atlasPage = spriteData.atlasPage;
        vbl.xzOffset.x = -halfX;
        vbl.xzOffset.y = 0.0f;
    }
    { // Bottom Right
        BillboardVertex& vbr = verts[1];
        vbr.rootPos.x = tilePosition.x;
        vbr.rootPos.y = tilePosition.y;
        vbr.rootPos.z = tilePosition.z;
        vbr.uvs.x = adjustedUvs.x + adjustedUvs.z;
        vbr.uvs.y = adjustedUvs.y + adjustedUvs.w;
        vbr.color = bottomColor;
        vbr.atlasPage = spriteData.atlasPage;
        vbr.xzOffset.x = halfX;
        vbr.xzOffset.y = 0.0f;
    }

    const f32 topZ = tilePosition.z + spriteData.dimsMeters.y;
    { // Top Left
        BillboardVertex& vtl = verts[2];
        vtl.rootPos.x = tilePosition.x;
        vtl.rootPos.y = tilePosition.y;
        vtl.rootPos.z = tilePosition.z;
        vtl.uvs.x = adjustedUvs.x;
        vtl.uvs.y = adjustedUvs.y;
        vtl.color = topColor;
        vtl.atlasPage = spriteData.atlasPage;
        vtl.xzOffset.x = -halfX;
        vtl.xzOffset.y = spriteData.dimsMeters.y;
    }
    { // Top Right
        BillboardVertex& vtr = verts[3];
        vtr.rootPos.x = tilePosition.x;
        vtr.rootPos.y = tilePosition.y;
        vtr.rootPos.z = tilePosition.z;
        vtr.uvs.x = adjustedUvs.x + adjustedUvs.z;
        vtr.uvs.y = adjustedUvs.y;
        vtr.color = topColor;
        vtr.atlasPage = spriteData.atlasPage;
        vtr.xzOffset.x = halfX;
        vtr.xzOffset.y = spriteData.dimsMeters.y;
    }
}

void addCross(std::vector<TileVertex>& vertexData, f32v3 cornerPosition, const SpriteData& spriteData, const f32v4& uvs, float width) {
    static constexpr float EPSILON = 0.005f;

    vertexData.resize(vertexData.size() + 8);
    TileVertex* verts = &vertexData.back() - 7;

    // Center the sprite
    // TODO: This shouldnt be hard coded to xy
    const f32v2 offset(-(float)((spriteData.dimsMeters.x - 1) / 2) + spriteData.offset.x, spriteData.offset.y);

    f32v4 adjustedUvs;
    if ((spriteData.flags & SPRITEDATA_FLAG_RAND_FLIP) && Random::getThreadSafef(cornerPosition.x, cornerPosition.y) > 0.5f) {
        // Flip horizontal
        adjustedUvs.x = uvs.x + uvs.z - UV_EPSILON;
        adjustedUvs.y = uvs.y + UV_EPSILON;
        adjustedUvs.z = -uvs.z + UV_EPSILON_2;
        adjustedUvs.w = uvs.w - UV_EPSILON_2;
    }
    else {
        adjustedUvs.x = uvs.x + UV_EPSILON;
        adjustedUvs.y = uvs.y + UV_EPSILON;
        adjustedUvs.z = uvs.z - UV_EPSILON_2;
        adjustedUvs.w = uvs.w - UV_EPSILON_2;
    }

    color4 topColor = color4((ui8)255u, (ui8)255u, (ui8)255u);
    color4 bottomColor = topColor;

    for (int i = 0; i < 2; ++i) {
        f32 offset = i * width;
        f32 invOffset = width - offset;
        { // Bottom Left
            TileVertex& vbl = *(verts++);
            vbl.pos = cornerPosition;
            vbl.uvs.x = adjustedUvs.x;
            vbl.uvs.y = adjustedUvs.y + adjustedUvs.w;
            vbl.color = bottomColor;
            vbl.atlasPage = spriteData.atlasPage;
            vbl.pos.y += offset;
        }
        { // Bottom Right
            TileVertex& vbr = *(verts++);
            vbr.pos = cornerPosition;
            vbr.uvs.x = adjustedUvs.x + adjustedUvs.z;
            vbr.uvs.y = adjustedUvs.y + adjustedUvs.w;
            vbr.color = bottomColor;
            vbr.atlasPage = spriteData.atlasPage;
            vbr.pos.x += width;
            vbr.pos.y += invOffset;
        }

        { // Top Left
            TileVertex& vtl = *(verts++);
            vtl.pos = cornerPosition;
            vtl.uvs.x = adjustedUvs.x;
            vtl.uvs.y = adjustedUvs.y;
            vtl.color = topColor;
            vtl.atlasPage = spriteData.atlasPage;
            vtl.pos.y += offset;
            vtl.pos.z += width;
        }
        { // Top Right
            TileVertex& vtr = *(verts++);
            vtr.pos = cornerPosition;
            vtr.uvs.x = adjustedUvs.x + adjustedUvs.z;
            vtr.uvs.y = adjustedUvs.y;
            vtr.color = topColor;
            vtr.atlasPage = spriteData.atlasPage;
            vtr.pos.x += width;
            vtr.pos.y += invOffset;
            vtr.pos.z += width;
        }
    }
}

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
    std::vector<TileVertex>& vertexData,
    const Chunk& chunk,
    const TileIndex& tileIndex,
    int layerIndex,
    const TileData& tileData,
    const SpriteData& spriteData
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

    for (int i = 0; i < 5; ++i) {
        float width = vmath::lerp(0.3f, 0.6f, Random::getCachedRandomf());
        addCross(vertexData, f32v3(tileWorldPos.x + Random::getCachedRandomf(), tileWorldPos.y + Random::getCachedRandomf(), tile.baseZPosition), spriteData, spriteData.uvs, width);
    }
        /*const int ITER_STEPS = 3;
        for (int i = 0; i < ITER_STEPS; ++i) {
            addQuad(vertexData, f32v3(tileWorldPos.x + (i % 2) / 16.0f, tileWorldPos.y + i / (float)ITER_STEPS, tile.baseZPosition), spriteData, spriteData.uvs);
        }*/
}

void addBlock(std::vector<TileVertex>& vertexData, TileShape shape, f32v3 tilePosition, const SpriteData& spriteData, const TileIndex& tileIndex, const Chunk& chunk, int layerIndex) {
    switch (spriteData.method) {
        case TileTextureMethod::SIMPLE: {
            // TODO: This shouldn't have to be hard coded to floor
            // We do not mesh TileShape::THIN here, it is a billboard
            if (shape == TileShape::BLOCK) {
                addQuad(vertexData, tilePosition + BOX_QUAD_FACING_GEOMETRY_OFFSETS[enum_cast(QuadFacing::TOP)], QuadFacing::TOP, spriteData, spriteData.uvs);
            }
            static_assert(enum_cast(TileShape::COUNT) == 2);
            break;
        }
        case TileTextureMethod::CONNECTED_WALL: {
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
                addQuad(vertexData, tileWorldPos + BOX_QUAD_FACING_GEOMETRY_OFFSETS[enum_cast(QuadFacing::TOP)], QuadFacing::TOP, spriteData, spriteData.uvs);
            }
            else {
                // Render top
                // Check if we need to render the base layer first
                if (data.a < 0x10) {
                    addQuad(vertexData, tileWorldPos + BOX_QUAD_FACING_GEOMETRY_OFFSETS[enum_cast(QuadFacing::TOP)], QuadFacing::TOP, spriteData, spriteData.uvs);
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
                    addQuad(vertexData, tilePosRoof, QuadFacing::TOP, spriteData, uvs);
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
                        addQuad(vertexData, quadPos, quadFacing, spriteData, uvs);

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
                            addQuad(vertexData, f32v3(quadPos.x, quadPos.y, quadPos.z - i), quadFacing, spriteData, uvs);
                        }
                    }
                }
            }
            break;
        }
        case TileTextureMethod::FLORA: {
            // We do not mesh flora in this pass
            break;
        }
    }
}


bool ChunkMesher::createMeshAsync(const Chunk& chunk) {
    
    TileMeshData* meshData = tryGetFreeTileMeshData();
    if (!meshData) {
        return false;
    }
    
    ++mNumMeshTasksRunning;
    chunk.mChunkRenderData.mIsBuildingBaseMesh = true;

    // TODO: Move somewhere else?
    chunk.mChunkRenderData.mMeshDirty = false;
    chunk.mChunkRenderData.mLODDirty = false;
    chunk.incRef();
    
    Services::Threadpool::ref().addTask([&chunk, meshData](ThreadPoolWorkerData* workerData) {
        ChunkRenderData& renderData = chunk.mChunkRenderData;
        const f32v2& chunkPos = chunk.getWorldPos();

        std::vector<TileVertex>& vertexData = meshData->mTileVertices;
        std::vector<BillboardVertex>& billboardVertexData = meshData->mBillboardVertices;
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

                    if (spriteData.method == TileTextureMethod::FLORA) {
                        // FLORA IS DONE IN SEPARATE PASS
                        continue;
                    }

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
                        f32v3 tilePosition(x + chunkPos.x, y + chunkPos.y, tile.baseZPosition);
                        addQuad(billboardVertexData, tilePosition, spriteData, spriteData.uvs);
                    }
                    else {
                        // Standard blocks
                        addBlock(vertexData, tileData.shape, f32v3(x + chunkPos.x, y + chunkPos.y, tile.baseZPosition), spriteData, index, chunk, layerIndex);
                    }
                }
            }
        }
    }, [this, &chunk, meshData]() {

        ChunkRenderData& renderData = chunk.mChunkRenderData;

        if (!renderData.mChunkMesh) {
            renderData.mChunkMesh = std::make_unique<QuadMesh>();
        }
        if (meshData->mTileVertices.size()) {
            QuadMesh& mesh = *renderData.mChunkMesh;
            mesh.setData(meshData->mTileVertices.data(), meshData->mTileVertices.size(), QuadMeshDrawMode::STATIC);
            meshData->mTileVertices.clear();
        }

        if (!renderData.mBillboardMesh) {
            renderData.mBillboardMesh = std::make_unique<BillboardMesh>();
        }
        if (meshData->mBillboardVertices.size()) {
            BillboardMesh& mesh = *renderData.mBillboardMesh;
            mesh.setData(meshData->mBillboardVertices.data(), meshData->mBillboardVertices.size(), QuadMeshDrawMode::DYNAMIC);
            meshData->mBillboardVertices.clear();
        }

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

    TileMeshData* meshData = tryGetFreeTileMeshData();
    if (!meshData) {
        return false;
    }

    ++mNumMeshTasksRunning;
    chunk.mChunkRenderData.mIsBuildingHighDetailFloraMesh = true;

    // TODO: Move somewhere else?
    chunk.mChunkRenderData.mHighDetailFloraMeshDirty = false;
    chunk.incRef();

    Services::Threadpool::ref().addTask([&chunk, meshData](ThreadPoolWorkerData* workerData) {
        ChunkRenderData& renderData = chunk.mChunkRenderData;
        const f32v2& chunkPos = chunk.getWorldPos();

        std::vector<TileVertex>& vertexData = meshData->mTileVertices;
        std::vector<BillboardVertex>& billboardVertexData = meshData->mBillboardVertices;

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
                    if (spriteData.method == TileTextureMethod::FLORA) {
                        addTileFlora(vertexData, chunk, index, layerIndex, tileData, spriteData);
                    }
                }
            }
        }
    }, [this, &chunk, meshData]() {

        ChunkRenderData& renderData = chunk.mChunkRenderData;

        if (!renderData.mHighDetailFloraMesh) {
            renderData.mHighDetailFloraMesh = std::make_unique<QuadMesh>();
        }
        if (meshData->mTileVertices.size()) {
            QuadMesh& mesh = *renderData.mHighDetailFloraMesh;
            mesh.setData(meshData->mTileVertices.data(), meshData->mTileVertices.size(), QuadMeshDrawMode::STATIC);
            meshData->mTileVertices.clear();
        }

        // Recycle and flag as free
        mFreeTileMeshData.push_back(meshData);
        chunk.mChunkRenderData.mIsBuildingBaseMesh = false;

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
