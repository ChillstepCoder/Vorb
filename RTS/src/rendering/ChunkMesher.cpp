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
constexpr float VERTICAL_OFFSET_PER_LAYER = 0.749f;
constexpr float AMBIENT_OCCLUSION_MULT = 0.7f;
constexpr float NO_AMBIENT_OCCLUSION = 1.0f;

static TileVertex sVertices[MAX_VERTICES_PER_CHUNK];

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

enum ExposedNeighborBits {
    BOTTOM_LEFT = 1 << 0,
    BOTTOM = 1 << 1,
    BOTTOM_RIGHT = 1 << 2,
    LEFT = 1 << 3,
    RIGHT = 1 << 4,
    TOP_LEFT = 1 << 5,
    TOP = 1 << 6,
    TOP_RIGHT = 1 << 7
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

void addQuad(TileVertex* verts, TileShape shape, f32v2 position, f32 zOffset, const SpriteData& spriteData, const f32v4& uvs, float extraHeight, float ambientOcclusionMult, float heightMult) {
    // Center the sprite
    const f32v2 offset(-(float)((spriteData.dimsMeters.x - 1) / 2) + spriteData.offset.x, spriteData.offset.y);
    position += offset;
    const float zLevelOffset = zOffset * VERTICAL_OFFSET_PER_LAYER;
    position.y += zLevelOffset;

    static constexpr float EPSILON = 0.005f;

    f32 topHeight = zOffset;
    f32 bottomHeight = zOffset;

    f32v4 adjustedUvs;
    if ((spriteData.flags & SPRITEDATA_FLAG_RAND_FLIP) && Random::getThreadSafef(position.x, position.y) > 0.5f) {
        // Flip horizontal
        adjustedUvs.x = uvs.x + uvs.z;
        adjustedUvs.y = uvs.y;
        adjustedUvs.z = -uvs.z;
        adjustedUvs.w = uvs.w;
    }
    else {
        adjustedUvs = uvs;
    }
    // Squish UVs to match the tile mesh for short walls and such that aren't 1.0f
    adjustedUvs.y += (1.0f - heightMult) * adjustedUvs.w;
    adjustedUvs.w *= heightMult;

    // 4 units per tile with max of 62 for size
    static constexpr float HEIGHT_SCALE = 4.0f;
    static constexpr float WALL_HEIGHT = 1.0f;

    float bottomDepth = extraHeight + zOffset;
    float topDepth = bottomDepth;
    ShadowState shadowTopLeft = ShadowState::LEFT;
    ShadowState shadowTopRight = ShadowState::RIGHT;
    ShadowState shadowBottomLeft = ShadowState::LEFT;
    ShadowState shadowBottomRight = ShadowState::RIGHT;
    color4 topColor = color4((ui8)255u, (ui8)255u, (ui8)255u);
    color4 bottomColor;
    float topRightDepthAdjust = 0.0f;
    // TODO: Encode height in the tile data, not as a guess
    switch (shape) {
        case TileShape::THIN: {
            topDepth += spriteData.dimsMeters.y;
            // This is actually fake ambient occlusion
            bottomColor = color4(ui8(255 * ambientOcclusionMult), ui8(255 * ambientOcclusionMult), ui8(255 * ambientOcclusionMult), 255u);
            shadowTopLeft = ShadowState::THIN;
            shadowTopRight = ShadowState::THIN;
            topHeight = spriteData.dimsMeters.y + zOffset;
            topRightDepthAdjust = EPSILON;
            break;
        }
        case TileShape::THICK: {
            topDepth += spriteData.dimsMeters.y;
            // This is actually fake ambient occlusion
            bottomColor = color4(ui8(255 * ambientOcclusionMult), ui8(255 * ambientOcclusionMult), ui8(255 * ambientOcclusionMult), 255u);
            topHeight = spriteData.dimsMeters.y + zOffset;
            break;
        }
        case TileShape::ROOF: {
            bottomDepth += WALL_HEIGHT;
            topDepth += WALL_HEIGHT;
            bottomColor = topColor;
            shadowBottomLeft = shadowTopLeft;
            shadowBottomRight = shadowTopRight;
            // Roof is always 1.0 high
            topHeight = 1.0f + zOffset;
            bottomHeight = topHeight;
            break;
        }
        case TileShape::FLOOR: {
            bottomColor = topColor;
            break;
        }
        default: {
            assert(false); // Missing
            break;
        }
    }
    static_assert((int)TileShape::COUNT == 4, "Update for new shape");


    ui8 topHeightUi8    = ui8(topHeight * HEIGHT_SCALE);
    ui8 bottomHeightUi8 = ui8(bottomHeight * HEIGHT_SCALE);

    { // Bottom Left
        TileVertex& vbl = verts[0];
        vbl.pos.x = position.x;
        vbl.pos.y = position.y;
        vbl.pos.z = bottomDepth;
        vbl.uvs.x = adjustedUvs.x;
        vbl.uvs.y = adjustedUvs.y + adjustedUvs.w;
        vbl.color = bottomColor;
        vbl.atlasPage = spriteData.atlasPage;
        vbl.shadowState = (ui8)shadowBottomLeft;
        vbl.height = bottomHeightUi8;
    }
    { // Bottom Right
        TileVertex& vbr = verts[1];
        vbr.pos.x = position.x + spriteData.dimsMeters.x + EPSILON;
        vbr.pos.y = position.y;
        vbr.pos.z = bottomDepth;
        vbr.uvs.x = adjustedUvs.x + adjustedUvs.z;
        vbr.uvs.y = adjustedUvs.y + adjustedUvs.w;
        vbr.color = bottomColor;
        vbr.atlasPage = spriteData.atlasPage;
        vbr.shadowState = (ui8)shadowBottomRight;
        vbr.height = bottomHeightUi8;
    }
    { // Top Left
        TileVertex& vtl = verts[2];
        vtl.pos.x = position.x;
        vtl.pos.y = position.y + spriteData.dimsMeters.y * heightMult + EPSILON;
        vtl.pos.z = topDepth;
        vtl.uvs.x = adjustedUvs.x;
        vtl.uvs.y = adjustedUvs.y;
        vtl.color = topColor;
        vtl.atlasPage = spriteData.atlasPage;
        vtl.shadowState = (ui8)shadowTopLeft;
        vtl.height = topHeightUi8;
    }
    { // Top Right
        TileVertex& vtr = verts[3];
        vtr.pos.x = position.x + spriteData.dimsMeters.x + EPSILON;
        vtr.pos.y = position.y + spriteData.dimsMeters.y * heightMult + EPSILON;
        vtr.pos.z = topDepth + topRightDepthAdjust; // Epsilon to prevent Z fighting with trees
        vtr.uvs.x = adjustedUvs.x + adjustedUvs.z;
        vtr.uvs.y = adjustedUvs.y;
        vtr.color = topColor;
        vtr.atlasPage = spriteData.atlasPage;
        vtr.shadowState = (ui8)shadowTopRight;
        vtr.height = topHeightUi8;
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
    const Chunk& chunk,
    const TileIndex& tileIndex,
    int layerIndex,
    const TileData& tileData,
    const SpriteData& spriteData,
    std::vector<TileVertex>& vertexData
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

    const int ITER_STEPS = 3;
    for (int i = 0; i < ITER_STEPS; ++i) {
        vertexData.resize(vertexData.size() + 4);
        addQuad(&vertexData.back() - 3, TileShape::THIN, f32v2(tileWorldPos.x + (i % 2) / 16.0f, tileWorldPos.y + i / (float)ITER_STEPS), tile.baseZPosition, spriteData, spriteData.uvs, layerDepth, NO_AMBIENT_OCCLUSION, 1.0f);
    }
}

void addTileConnected(
    const Chunk& chunk,
    const TileIndex& tileIndex,
    int layerIndex,
    const TileData& tileData,
    const SpriteData& spriteData, 
    std::vector<TileVertex>& vertexData
) {
    const float layerDepth = layerIndex * LAYER_DEPTH_ADD;
    const Tile& tile = chunk.getTileAtNoAssert(tileIndex);
    const TileID tileId = tile.layers[layerIndex];

    const f32v2 tileWorldPos = chunk.getWorldPos() + f32v2(tileIndex.getX(), tileIndex.getY());

    Tile neighbors[8];
    chunk.getTileNeighbors(tileIndex, neighbors);

    const int zPosition = tile.baseZPosition + ((spriteData.flags & SPRITEDATA_FLAG_OPAQUE) ? 1 : 0);
    const int bottomHeightDiff = zPosition - getTileHeight(neighbors[(int)NeighborIndex::BOTTOM], layerIndex);
    const int topHeightDiff    = zPosition - getTileHeight(neighbors[(int)NeighborIndex::TOP], layerIndex);

    unsigned exposedBits = 0;
    for (int i = 0; i < 8; ++i) {
        const Tile& neighbor = neighbors[i];
        // If neighbor is different tile, or is lower than us, we are exposed to this neighbor
        if (neighbor.layers[layerIndex] != tileId || neighbor.baseZPosition < tile.baseZPosition ){
            exposedBits |= (1 << i);
        }
    }
    const ConnectedWallData& data = sConnectedWallData[exposedBits];
    if (data.data == 0) {
        vertexData.resize(vertexData.size() + 4);
        addQuad(&vertexData.back() - 3, TileShape::ROOF, f32v2(tileWorldPos.x, tileWorldPos.y + VERTICAL_OFFSET_PER_LAYER), tile.baseZPosition, spriteData, spriteData.uvs, layerDepth, NO_AMBIENT_OCCLUSION, 1.0f);
    }
    else {
        // Check if we need to render the base layer first
        if (data.a < 0x10) {
            vertexData.resize(vertexData.size() + 4);
            addQuad(&vertexData.back() - 3, TileShape::ROOF, f32v2(tileWorldPos.x, tileWorldPos.y + VERTICAL_OFFSET_PER_LAYER), tile.baseZPosition, spriteData, spriteData.uvs, layerDepth, NO_AMBIENT_OCCLUSION, 1.0f);
        }
        // Render up to 4 textures depending on configuration
        for (int i = 0; i < 4; ++i) {
            const ui16 val = data.dataArray[i];
            if (val == 0) break;

            const f32v2 offsets = getUvsOffsetsFromConnectedWallIndex(val);
            f32v4 uvs = spriteData.uvs;
            uvs.x += offsets.x * uvs.z;
            uvs.y += offsets.y * uvs.w;
            vertexData.resize(vertexData.size() + 4);
            addQuad(&vertexData.back() - 3, TileShape::ROOF, f32v2(tileWorldPos.x, tileWorldPos.y + VERTICAL_OFFSET_PER_LAYER), tile.baseZPosition, spriteData, uvs, layerDepth + LAYER_DEPTH_ADD, NO_AMBIENT_OCCLUSION, 1.0f);
        }
        // Render exposed wall bottom if needed
        if (isBitSet(exposedBits, BOTTOM)) {
            // Simple 2 bit LUT
            const unsigned sideCheck = ((exposedBits & (LEFT | RIGHT)) >> 3);
            const ui16 val = sExposedWallLookup[sideCheck];

            const f32v2 offsets = getUvsOffsetsFromConnectedWallIndex(val);
            f32v4 uvs = spriteData.uvs;
            uvs.x += offsets.x * uvs.z;
            uvs.y += offsets.y * uvs.w;
            const bool ambientOcclusion = bottomHeightDiff > 1 ? NO_AMBIENT_OCCLUSION : AMBIENT_OCCLUSION_MULT;
            vertexData.resize(vertexData.size() + 4);
            addQuad(&vertexData.back() - 3, TileShape::THICK, f32v2(tileWorldPos.x, tileWorldPos.y), tile.baseZPosition, spriteData, uvs, layerDepth, ambientOcclusion, VERTICAL_OFFSET_PER_LAYER);
        }
    }

    // See if we need to add additional "tower" quads if we are exposed deeper on the bottom
    const float maxHeightDiff = std::max(bottomHeightDiff, topHeightDiff);
    for (int i = 1; i < maxHeightDiff; ++i) {
        exposedBits = 0;
        const Tile& leftNeighbor  = neighbors[(int)NeighborIndex::LEFT];
        const Tile& rightNeighbor = neighbors[(int)NeighborIndex::RIGHT];
        int adjustedZPosition = tile.baseZPosition - i;
        if (leftNeighbor.baseZPosition < adjustedZPosition || leftNeighbor.layers[layerIndex] != tileId) {
            exposedBits |= LEFT;
        }
        if (rightNeighbor.baseZPosition < adjustedZPosition || rightNeighbor.layers[layerIndex] != tileId) {
            exposedBits |= RIGHT;
        }
        // New exposure check for left and right on towers
        const unsigned sideCheck = (exposedBits >> 3);
        const ui16 val = sExposedWallLookup[sideCheck];

        const f32v2 offsets = getUvsOffsetsFromConnectedWallIndex(val);
        f32v4 uvs = spriteData.uvs;
        uvs.x += offsets.x * uvs.z;
        // + 1 for the tall wall variants
        uvs.y += (offsets.y + 1) * uvs.w;
        // TODO: this resize here...

        // TODO: Stretched quads?
        if (i < bottomHeightDiff) {
            vertexData.resize(vertexData.size() + 4);
            const float ambientOcclusion = (i == bottomHeightDiff - 1) ? AMBIENT_OCCLUSION_MULT : NO_AMBIENT_OCCLUSION;
            addQuad(&vertexData.back() - 3, TileShape::THICK, f32v2(tileWorldPos.x, tileWorldPos.y), tile.baseZPosition - i, spriteData, uvs, layerDepth, ambientOcclusion, VERTICAL_OFFSET_PER_LAYER);
        }
        if (i < topHeightDiff) {
            vertexData.resize(vertexData.size() + 4);
            const float ambientOcclusion = (i == bottomHeightDiff - 1) ? AMBIENT_OCCLUSION_MULT : NO_AMBIENT_OCCLUSION;
            addQuad(&vertexData.back() - 3, TileShape::THICK, f32v2(tileWorldPos.x, tileWorldPos.y + 1.0f), tile.baseZPosition - i, spriteData, uvs, layerDepth, ambientOcclusion, VERTICAL_OFFSET_PER_LAYER);
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
    chunk.mChunkRenderData.mBaseDirty = false;
    chunk.mChunkRenderData.mLODDirty = false;
    chunk.incRef();
    
    Services::Threadpool::ref().addTask([&chunk, meshData](ThreadPoolWorkerData* workerData) {
        ChunkRenderData& renderData = chunk.mChunkRenderData;
        const f32v2& chunkPos = chunk.getWorldPos();

        std::vector<TileVertex>& vertexData = meshData->mTileVertices;
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
                    switch (spriteData.method) {
                        case TileTextureMethod::SIMPLE: {
                            vertexData.resize(vertexData.size() + 4);
                            addQuad(&vertexData.back() - 3, tileData.shape, f32v2(x + chunkPos.x, y + chunkPos.y), tile.baseZPosition, spriteData, spriteData.uvs, layerIndex * LAYER_DEPTH_ADD, AMBIENT_OCCLUSION_MULT, 1.0f);
                            break;
                        }
                        case TileTextureMethod::CONNECTED_WALL: {
                            addTileConnected(chunk, index, layerIndex, tileData, spriteData, vertexData);
                            break;
                        }
                        case TileTextureMethod::FLORA: {
                            // We do not mesh flora in this pass
                            break;
                        }
                    }
                }
            }
        }
    }, [this, &chunk, meshData]() {

        ChunkRenderData& renderData = chunk.mChunkRenderData;

        // Mesh
        if (!renderData.mChunkMesh) {
            renderData.mChunkMesh = std::make_unique<QuadMesh>();
        }
        QuadMesh& mesh = *renderData.mChunkMesh;
        mesh.setData(meshData->mTileVertices.data(), meshData->mTileVertices.size(), mTextureAtlas.getAtlasTexture(), QuadMeshDrawMode::STATIC);
        meshData->mTileVertices.clear();

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

bool ChunkMesher::createHighDetailFloraMeshAsync(const Chunk& chunk)
{
    TileMeshData* meshData = tryGetFreeTileMeshData();
    if (!meshData) {
        return false;
    }

    ++mNumMeshTasksRunning;
    chunk.mChunkRenderData.mIsBuildingFloraMesh = true;

    // TODO: Move somewhere else?
    chunk.mChunkRenderData.mFloraDirty = false;
    chunk.incRef();

    Services::Threadpool::ref().addTask([&chunk, meshData](ThreadPoolWorkerData* workerData) {
        ChunkRenderData& renderData = chunk.mChunkRenderData;
        const f32v2& chunkPos = chunk.getWorldPos();

        std::vector<TileVertex>& vertexData = meshData->mTileVertices;

        // Flora must be MID layer
        constexpr int layerIndex = enum_cast(TileLayer::Mid);

        for (int y = 0; y < CHUNK_WIDTH; ++y) {
            for (int x = 0; x < CHUNK_WIDTH; ++x) {
                //  TODO: Multiple world layers
                TileIndex index(x, y);
                const Tile& tile = chunk.mTiles[index];
                TileID layerTile = tile.layers[layerIndex];
                if (layerTile == TILE_ID_NONE) {
                    continue;
                }
                const TileData& tileData = TileRepository::getTileData(layerTile);
                const SpriteData& spriteData = tileData.spriteData;

                // Flora mesh ONLY
                if (spriteData.method == TileTextureMethod::FLORA) {
                    addTileFlora(chunk, index, layerIndex, tileData, spriteData, vertexData);
                }
            }
        }
    }, [this, &chunk, meshData]() {

        ChunkRenderData& renderData = chunk.mChunkRenderData;

        // Mesh
        if (!renderData.mFloraMesh) {
            renderData.mFloraMesh = std::make_unique<QuadMesh>();
        }
        QuadMesh& mesh = *renderData.mFloraMesh;
        mesh.setData(meshData->mTileVertices.data(), meshData->mTileVertices.size(), mTextureAtlas.getAtlasTexture(), QuadMeshDrawMode::DYNAMIC);
        meshData->mTileVertices.clear();

        // LOD
        // uploadLODTexture(renderData, meshData->mLODTexturePixelBuffer);

        // Recycle and flag as free
        mFreeTileMeshData.push_back(meshData);
        chunk.mChunkRenderData.mIsBuildingFloraMesh = false;

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
