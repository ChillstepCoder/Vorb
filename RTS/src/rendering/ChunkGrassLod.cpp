#include "stdafx.h"
#include "ChunkGrassLod.h"

#include "rendering/QuadMesh.h"
#include "world/Chunk.h"
#include "camera/Camera3D.h"

#include "options/DebugOptions.h"

#include "services/Services.h"

#include "generation/WorldGenerationData.h"
#include <Vorb/graphics/GLProgram.h>

#include "Random.h"
#include "DebugRenderer.h"

const f32 LOG_MULT = (f32)(1.0 / (2 * log(2)));

constexpr int GRASS_LOD_DETAIL[MAX_GRASS_LOD_DEPTH] = {
    0,
    1,
    2,
    4,
    8,
};

constexpr f32 GRASS_BLADE_WIDTHS[MAX_GRASS_LOD_DEPTH] = {
    0.0f,
    0.5f,
    0.25f,
    0.1f,
    0.05f,
};

constexpr f32 GRASS_SUBDIVIDE_DISTANCES_SQ[MAX_GRASS_LOD_DEPTH] = { // sqrt(pow(WIDTH, 2) * 2) for diagonal distance widths
    FLT_MAX,
    SQ(91.0f),
    SQ(46.0f),
    SQ(23.0f),
    -FLT_MAX // Never subdivide last
};

// Generated via (log(3 * i + 1) / (2 * log(2)))
constexpr ui32 GRASS_LOD_FROM_INDEX[QUADTREE_SIZE] = {
    0u, 1u, 1u, 1u, 1u, 2u, 2u, 2u, 2u, 2u, 2u, 2u, 2u, 2u, 2u, 2u, 2u, 2u, 2u, 2u, 2u,
    3u, 3u, 3u, 3u, 3u, 3u, 3u, 3u, 3u, 3u, 3u, 3u, 3u, 3u, 3u, 3u, 3u, 3u, 3u, 3u, 3u,
    3u, 3u, 3u, 3u, 3u, 3u, 3u, 3u, 3u, 3u, 3u, 3u, 3u, 3u, 3u, 3u, 3u, 3u, 3u, 3u, 3u,
    3u, 3u, 3u, 3u, 3u, 3u, 3u, 3u, 3u, 3u, 3u, 3u, 3u, 3u, 3u, 3u, 3u, 3u, 3u, 3u, 3u,
    3u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u,
    4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u,
    4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u,
    4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u,
    4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u,
    4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u,
    4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u,
    4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u,
    4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u,
    4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u,
    4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u,
    4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u, 4u,
    4u, 4u, 4u, 4u, 4u,
};
static_assert(MAX_GRASS_LOD_DEPTH == 5);

// Algorithmically calculated then dumped here
const ui32v2 GRASS_PATCH_POSITIONS[QUADTREE_SIZE] = {
    {0,0}, {0,0}, {64,0}, {0,64}, {64,64}, {0,0}, {32,0}, {0,32}, {32,32}, {64,0}, {96,0}, {64,32},
    {96,32}, {0,64}, {32,64}, {0,96}, {32,96}, {64,64}, {96,64}, {64,96}, {96,96}, {0,0}, {16,0},
    {0,16}, {16,16}, {32,0}, {48,0}, {32,16}, {48,16}, {0,32}, {16,32}, {0,48}, {16,48}, {32,32},
    {48,32}, {32,48}, {48,48}, {64,0}, {80,0}, {64,16}, {80,16}, {96,0}, {112,0}, {96,16}, {112,16},
    {64,32}, {80,32}, {64,48}, {80,48}, {96,32}, {112,32}, {96,48}, {112,48}, {0,64}, {16,64}, {0,80},
    {16,80}, {32,64}, {48,64}, {32,80}, {48,80}, {0,96}, {16,96}, {0,112}, {16,112}, {32,96}, {48,96},
    {32,112}, {48,112}, {64,64}, {80,64}, {64,80}, {80,80}, {96,64}, {112,64}, {96,80}, {112,80}, {64,96},
    {80,96}, {64,112}, {80,112}, {96,96}, {112,96}, {96,112}, {112,112}, {0,0}, {8,0}, {0,8}, {8,8}, {16,0},
    {24,0}, {16,8}, {24,8}, {0,16}, {8,16}, {0,24}, {8,24}, {16,16}, {24,16}, {16,24}, {24,24}, {32,0},
    {40,0}, {32,8}, {40,8}, {48,0}, {56,0}, {48,8}, {56,8}, {32,16}, {40,16}, {32,24}, {40,24}, {48,16},
    {56,16}, {48,24}, {56,24}, {0,32}, {8,32}, {0,40}, {8,40}, {16,32}, {24,32}, {16,40}, {24,40}, {0,48},
    {8,48}, {0,56}, {8,56}, {16,48}, {24,48}, {16,56}, {24,56}, {32,32}, {40,32}, {32,40}, {40,40}, {48,32},
    {56,32}, {48,40}, {56,40}, {32,48}, {40,48}, {32,56}, {40,56}, {48,48}, {56,48}, {48,56}, {56,56}, {64,0},
    {72,0}, {64,8}, {72,8}, {80,0}, {88,0}, {80,8}, {88,8}, {64,16}, {72,16}, {64,24}, {72,24}, {80,16},
    {88,16}, {80,24}, {88,24}, {96,0}, {104,0}, {96,8}, {104,8}, {112,0}, {120,0}, {112,8}, {120,8}, {96,16},
    {104,16}, {96,24}, {104,24}, {112,16}, {120,16}, {112,24}, {120,24}, {64,32}, {72,32}, {64,40}, {72,40},
    {80,32}, {88,32}, {80,40}, {88,40}, {64,48}, {72,48}, {64,56}, {72,56}, {80,48}, {88,48}, {80,56},
    {88,56}, {96,32}, {104,32}, {96,40}, {104,40}, {112,32}, {120,32}, {112,40}, {120,40}, {96,48}, {104,48},
    {96,56}, {104,56}, {112,48}, {120,48}, {112,56}, {120,56}, {0,64}, {8,64}, {0,72}, {8,72}, {16,64},
    {24,64}, {16,72}, {24,72}, {0,80}, {8,80}, {0,88}, {8,88}, {16,80}, {24,80}, {16,88}, {24,88}, {32,64},
    {40,64}, {32,72}, {40,72}, {48,64}, {56,64}, {48,72}, {56,72}, {32,80}, {40,80}, {32,88}, {40,88},
    {48,80}, {56,80}, {48,88}, {56,88}, {0,96}, {8,96}, {0,104}, {8,104}, {16,96}, {24,96}, {16,104}, {24,104},
    {0,112}, {8,112}, {0,120}, {8,120}, {16,112}, {24,112}, {16,120}, {24,120}, {32,96}, {40,96}, {32,104},
    {40,104}, {48,96}, {56,96}, {48,104}, {56,104}, {32,112}, {40,112}, {32,120}, {40,120}, {48,112}, {56,112},
    {48,120}, {56,120}, {64,64}, {72,64}, {64,72}, {72,72}, {80,64}, {88,64}, {80,72}, {88,72}, {64,80},
    {72,80}, {64,88}, {72,88}, {80,80}, {88,80}, {80,88}, {88,88}, {96,64}, {104,64}, {96,72}, {104,72}, {112,64},
    {120,64}, {112,72}, {120,72}, {96,80}, {104,80}, {96,88}, {104,88}, {112,80}, {120,80}, {112,88},
    {120,88}, {64,96}, {72,96}, {64,104}, {72,104}, {80,96}, {88,96}, {80,104}, {88,104}, {64,112}, {72,112},
    {64,120}, {72,120}, {80,112}, {88,112}, {80,120}, {88,120}, {96,96}, {104,96}, {96,104}, {104,104}, {112,96},
    {120,96}, {112,104}, {120,104}, {96,112}, {104,112}, {96,120}, {104,120}, {112,112}, {120,112}, {112,120}, {120,120}
};
static_assert(MAX_GRASS_LOD_DEPTH == 5);

const ui32v2 LOD_DIMS[MAX_GRASS_LOD_DEPTH] = {
    ui32v2(CHUNK_WIDTH),
    ui32v2(CHUNK_WIDTH >> 1),
    ui32v2(CHUNK_WIDTH >> 2),
    ui32v2(CHUNK_WIDTH >> 3),
    ui32v2(CHUNK_WIDTH >> 4),
};

const f32v2 LOD_HALF_DIMS[MAX_GRASS_LOD_DEPTH] = {
    f32v2(CHUNK_WIDTH / 2.0f),
    f32v2((CHUNK_WIDTH >> 1) / 2.0f),
    f32v2((CHUNK_WIDTH >> 2) / 2.0f),
    f32v2((CHUNK_WIDTH >> 3) / 2.0f),
    f32v2((CHUNK_WIDTH >> 4) / 2.0f),
};

const f32 LOD_RADIUS_DIMS[MAX_GRASS_LOD_DEPTH] = {
    f32(sqrt(pow(CHUNK_WIDTH / 2.0f, 2.0f) * 2.0f)),
    f32(sqrt(pow((CHUNK_WIDTH >> 1) / 2.0f, 2.0f) * 2.0f)),
    f32(sqrt(pow((CHUNK_WIDTH >> 2) / 2.0f, 2.0f) * 2.0f)),
    f32(sqrt(pow((CHUNK_WIDTH >> 3) / 2.0f, 2.0f) * 2.0f)),
    f32(sqrt(pow((CHUNK_WIDTH >> 4) / 2.0f, 2.0f) * 2.0f)),
};

const ui32v2 CHILD_OFFSETS[4] = {
    ui32v2(0, 0), // Bottom left
    ui32v2(1, 0), // Bottom Right
    ui32v2(0, 1), // Top left
    ui32v2(1, 1), // Top Right
};

constexpr ui32 LEVEL_SIZES[MAX_GRASS_LOD_DEPTH] = {
    1,   //1^2
    4,   //2^2
    16,  //4^2
    64,  //8^2
    256, //16^2
};
constexpr ui32 LEVEL_WIDTHS[MAX_GRASS_LOD_DEPTH] = {
    1,
    2,
    4,
    8,
    16,
};

inline ui32 getParentIndex(ui32 index) {
    assert(index != 0);
    return (index - 1u) / 4u;
}

inline ChunkGrassPatch& getParent(ui32 index, ChunkGrassPatch nodes[]) {
    assert(index != 0);
    return nodes[(index - 1u) / 4u];
}

ChunkGrassLod::ChunkGrassLod(const Chunk& chunk) : mChunk(chunk) {
    mNodes[0].init();
    mNodes[0].mFlags &= (~GRASS_PATCH_FLAG_DIRTY_MESH);
    mActiveNodes[0] = 0;
    mNumActiveNodes = 1;

    mChunk.incRef();
}


ChunkGrassLod::~ChunkGrassLod()
{
    mChunk.decRef();
}

bool isPatchInRange(const f32v2& centerPos, const f32v2& cameraPos, f32 radius) {
    return (length2(centerPos - cameraPos) - SQ(radius)) <= sDebugOptions.mGrassDistanceSq - SQ(CHUNK_WIDTH * 0.5f); // SQ chunkwidth half will make it fit more closely (for some reason?)
}


void ChunkGrassLod::render(const Camera3D& camera, const vg::GLProgram& program) {
    const f32v3& cameraPos = camera.getPosition();
    const f32v2 cameraPos2Drelative = f32v2(cameraPos.x, cameraPos.y) - mChunk.getWorldPos();
    VGUniform crossfadeAlphaUniform = program.getUniform("unCrossfadeAlpha"); // TODO: Cache?
    VGUniform crossfadeDirectionUniform = program.getUniform("unCrossfadeDirection");
    f32v3 pos = mChunk.getWorldPos3D();
    for (ui32 i = 0; i < mNumActiveNodes; ++i) {
        ui32 index = mActiveNodes[i];
        ChunkGrassPatch& patch = mNodes[index];

        if (patch.canRender()) {
            auto& mesh = mMeshes[index];
            ui32 lod = GRASS_LOD_FROM_INDEX[index];
            f32v2 centerPos = f32v2(GRASS_PATCH_POSITIONS[index]) + LOD_HALF_DIMS[lod];
            f32v3 centerPos3d(centerPos.x, centerPos.y, 0.0f);
            if (patch.isCrossfading()) {
                glUniform1f(crossfadeAlphaUniform, mCrossfadeTable[patch.mCrossFadeTableIndex] * 0.5f /* Constant that was selected via trial and error*/);
                glUniform1f(crossfadeDirectionUniform, patch.mFlags & GRASS_PATCH_FLAG_CROSSFADING_IN ? 1.0f : 0.0f);
            }
            else {
                glUniform1f(crossfadeAlphaUniform, 0.0f);
                glUniform1f(crossfadeDirectionUniform, 0.0f);
            }
            const f32 radius = LOD_RADIUS_DIMS[lod];
            if (camera.sphereIsVisible(centerPos3d + pos, radius) &&
                isPatchInRange(centerPos, cameraPos2Drelative, radius)) {
                mesh->draw(program);
            }
        }
    }
}

void ChunkGrassLod::renderDebug(const Camera3D& camera) {

    const f32v3& cameraPos = camera.getPosition();
    const f32v2 cameraPos2Drelative = f32v2(cameraPos.x, cameraPos.y) - mChunk.getWorldPos();

    f32v3 pos = mChunk.getWorldPos3D();
    for (ui32 i = 0; i < mNumActiveNodes; ++i) {
        ui32 index = mActiveNodes[i];
        ChunkGrassPatch& patch = mNodes[index];
        ui32 lod = GRASS_LOD_FROM_INDEX[index];
        // TODO: Dont check this since we will never have pure root

        color4 color = color4(1.0f, 1.0f, 1.0f);
        f32v2 centerPos = f32v2(GRASS_PATCH_POSITIONS[index]) + LOD_HALF_DIMS[lod];
        if (!isPatchInRange(centerPos, cameraPos2Drelative, LOD_RADIUS_DIMS[lod])) {
            color = color4(1.0f, 0.0f, 0.0f);
        }
        else {
            switch (patch.mStatus) {
                case GRASS_PATCH_STATUS_INVALID: color = color4(0.0f, 1.0f, 0.0f); break;
                case GRASS_PATCH_STATUS_VALID: color = color4(0.0f, 0.0f, 1.0f); break;
                case GRASS_PATCH_STATUS_RECOMBINING: color = color4(0.0f, 1.0f, 1.0f); break;
                case GRASS_PATCH_STATUS_WAITING_PARENT_RECOMBINE: color = color4(1.0f, 1.0f, 0.0f); break;
                case GRASS_PATCH_STATUS_SUBDIVIDED: color = color4(0.0f, 0.0f, 0.0f); break;
                case GRASS_PATCH_STATUS_WAITING_FOR_CHILD_RECOMBINE_0: color = color4(0.2f, 0.0f, 1.0f); break;
                case GRASS_PATCH_STATUS_WAITING_FOR_CHILD_RECOMBINE_1: color = color4(0.4f, 0.0f, 1.0f); break;
                case GRASS_PATCH_STATUS_WAITING_FOR_CHILD_RECOMBINE_2: color = color4(0.6f, 0.0f, 1.0f); break;
                case GRASS_PATCH_STATUS_READY_TO_RECOMBINE: color = color4(1.0f, 0.0f, 1.0f); break;
            }
        }
        if (lod != 0) {
            ui32v2 posOffset = GRASS_PATCH_POSITIONS[index];
            DebugRenderer::drawWireQuad(pos + f32v3(posOffset.x, posOffset.y, 0.0f), f32v2(LOD_DIMS[lod]), color);

            if (patch.isCrossfading()) {
                const f32v2 halfDims = f32v2(LOD_DIMS[lod]) * 0.5f;
                DebugRenderer::drawWireQuad(pos + f32v3(posOffset.x + halfDims.x, posOffset.y + halfDims.y, 0.0f) , halfDims, color4(1.0f, 0.0f, 1.0f));
            }
        }
    }
}

void createGrassMesh(
    GrassBillboardMesh& grassMesh,
    const Chunk& chunk,
    const ui32v2& tilePosStart,
    ui32 lod
) {
    const ui32v2& dims = LOD_DIMS[lod];
    const ui32 density = GRASS_LOD_DETAIL[lod];
    const f32 bladeWidth = GRASS_BLADE_WIDTHS[lod];
    grassMesh.reserveQuadCount((size_t)dims.x * dims.y * density * density);
    for (ui32 y = 0; y < dims.y; ++y) {
        for (ui32 x = 0; x < dims.x; ++x) {
            TileIndex tileIndex(tilePosStart.x + x, tilePosStart.y + y);

            ui8 grassVal = chunk.getGrassAt(tileIndex);
            if (grassVal == 0) {
                continue;
            }

            const f32v2 tileWorldPos = f32v2(tileIndex.getX(), tileIndex.getY());

            /*Tile neighbors[8];
            chunk.getTileNeighbors(tileIndex, neighbors);

            const int zPosition = tile.baseZPosition + ((spriteData.flags & SPRITEDATA_FLAG_OPAQUE) ? 1 : 0);
            const int bottomHeightDiff = zPosition - getTileHeight(neighbors[(int)NeighborIndex::BOTTOM], layerIndex);
            const int topHeightDiff = zPosition - getTileHeight(neighbors[(int)NeighborIndex::TOP], layerIndex);*/

            const int tx = tileIndex.getX();
            const int ty = tileIndex.getY();
            // Allow overlap when adjacent tiles are the same
            //const float rightXMult = (rightTile.baseZPosition != tile.baseZPosition || tileId != rightTile.layers[layerIndex]) ? 1.0f : 0.0f;
            //const float topXMult = (topTile.baseZPosition != tile.baseZPosition || tileId != topTile.layers[layerIndex]) ? 1.0f : 0.0f;

            // Handle variant UVs
            constexpr int NUM_GRASS_TYPES = 4;

            // Determine edge


            // Generate blades
            for (int y2 = 0; y2 < (int)density; ++y2) {
                for (int x2 = 0; x2 < (int)density; ++x2) {
                    const f32 rnd = Random::getCachedRandomfSpecific(x2 + CHUNK_SIZE * y2 - tx - ty * CHUNK_SIZE) * 0.9f;
                    const float xo = (x2 + rnd) / (float)density;
                    const float yo = (y2 - rnd) / (float)density;
                    const float rsize = lerp(0.4f, 0.6f, rnd);
                    const f32 grassNoise = -sWorldGenData.mGrassNoise.compute((f64)tileWorldPos.x + xo + chunk.getWorldPos().x, (f64)tileWorldPos.y + yo + chunk.getWorldPos().y);
                    const ui8 variantIndex = (ui8)((grassNoise + 1.0f) * SQ(NUM_GRASS_TYPES)) % NUM_GRASS_TYPES;
                    grassMesh.addBladeQuad(
                        f32v3(tileWorldPos.x + xo, tileWorldPos.y + yo, 0.0f), // TODO: new height
                        f32v2(bladeWidth, rsize),
                        variantIndex
                    );
                }
            }
        }
    }
};

void ChunkGrassLod::update(const f32v2& loadCenter)
{
    f32v2 mRelativeCenter = loadCenter - mChunk.getWorldPos();
    bool needSort = false;
    for (ui32 i = 0; i < mNumActiveNodes;) {
        ui32 index = mActiveNodes[i];
        ChunkGrassPatch& patch = mNodes[index];

        assert(patch.isActive());

        if (patch.isCrossfading()) {
            // when crossfading, we crossfade until we are complete
            assert(!patch.isMeshing());
            f32 currentCrossfade = mCrossfadeTable[patch.mCrossFadeTableIndex];
            if (currentCrossfade >= 1.0f) {
                if (patch.mFlags & GRASS_PATCH_FLAG_CROSSFADING_OUT) {
                    if (currentCrossfade >= 1.0f) {
                        // Free mesh
                        mMeshes[index].reset();
                        // Destroy and continue
                        if (patch.didSignalRecombine()) {
                            // We are combining into parent
                            patch.destroy(GRASS_PATCH_STATUS_INVALID);
                        }
                        else {
                            // We are subdividing into children
                            patch.destroy(GRASS_PATCH_STATUS_SUBDIVIDED);
                        }
                        mActiveNodes[i] = mActiveNodes[--mNumActiveNodes];
                        needSort = true;
                        continue;
                    }
                }
                else {
                    patch.mFlags &= (~GRASS_PATCH_IS_CROSSFADING);
                }
            }
            ++i;
            continue;
        }
        else if (patch.isMeshing()) {
            // When patches are meshing, we wait for them to complete
            ++i;
            continue;
        }
        else if (patch.mStatus == GRASS_PATCH_STATUS_WAITING_PARENT_RECOMBINE) {
            // Waiting on parent to mesh and stuff, do nothing
            ++i;
            continue;
        }

        // For node I, its children are 4 * i + 1 through 4 * i + 4
        // Therefore for node I, its parent is (i - 1) / 4;
        ui32 lod = GRASS_LOD_FROM_INDEX[index];
        assert(!patch.isCrossfading());
        // If our parent is active, we will do nothing but mesh, since the parent is either waiting on us to mesh, or is recombining us
        if (lod > 0 && getParent(index, mNodes).isActive()) {
            assert(patch.mStatus != GRASS_PATCH_STATUS_SUBDIVIDED);
            if (patch.isMeshDirty()) {
                updateMeshForPatch(patch, lod, index);
            }
            ++i;
            continue;
        }
        else if (patch.mStatus == GRASS_PATCH_STATUS_RECOMBINING) {
            if (patch.isMeshDirty()) {
                updateMeshForPatch(patch, lod, index);
                ++i; // Move to next
            }
            else if (!patch.isMeshing()) {
                // We can recombine
                patch.mStatus = GRASS_PATCH_STATUS_VALID;
                // Start crossfade
                mCrossfadeTable[index] = 0.0f;
                mCrossfadeActiveTable[mNumCrossfading++] = index;
                patch.initiateCrossfadeIn(index);
                ui16 childIndexFirst = 4u * index + 1;
                ui16 childIndexLast = 4u * index + 4;
                // Crossfade children
                for (ui16 j = childIndexFirst; j <= childIndexLast; ++j) {
                    mNodes[j].initiateCrossfadeOut(index);
                }
                // Don't move to next
            }
            continue; 
        }

        f32v2 centerPos = f32v2(GRASS_PATCH_POSITIONS[index]) + LOD_HALF_DIMS[lod];
            
        f32 distance2 = glm::distance2(centerPos, mRelativeCenter);
        if (distance2 < GRASS_SUBDIVIDE_DISTANCES_SQ[lod] + SQ(sDebugOptions.mGrassLodDistanceOffset)) {
            // We want to subdivide
            if (patch.mStatus == GRASS_PATCH_STATUS_SUBDIVIDED) {
                // If we reach here we are still active and waiting on children, check if our
                // children are finished meshing
                if (patch.areChildrenDoneMeshing(index, mNodes)) {
                    // Start crossfade
                    mCrossfadeTable[index] = 0.0f;
                    mCrossfadeActiveTable[mNumCrossfading++] = index;
                    // Tell children they can draw
                    ui16 childIndexFirst = 4u * index + 1;
                    for (ui16 i = 0; i < 4; ++i) {
                        ChunkGrassPatch& child = mNodes[childIndexFirst + i];
                        child.initiateCrossfadeIn(index);
                    }
                    patch.initiateCrossfadeOut(index);
                    continue;
                }
                ++i;
                continue;
            }

            // If we previously signaled parent to recombine, unsignal it
            if (patch.didSignalRecombine()) {
                patch.trySignalParentNoLongerDesireRecombine(index, mNodes);
            }

            // If we were invalid, then there is no waiting to be done, mark us as invalid
            if (patch.mStatus == GRASS_PATCH_STATUS_INVALID) {
                // Pop and swap
                patch.destroy();
                mActiveNodes[i] = mActiveNodes[--mNumActiveNodes];
                patch.mStatus = GRASS_PATCH_STATUS_SUBDIVIDED;
                needSort = true;
                // Do not increment to next patch
            }
            else if (patch.mStatus == GRASS_PATCH_STATUS_VALID) {
                // Subdivide, we are still active, but we are waiting for our children to finish initializing
                patch.mStatus = GRASS_PATCH_STATUS_SUBDIVIDED;
                ++i; // Increment to next patch
            }
            else {
                // Else we are currently waiting for children to recombine
                ++i; // Increment to next patch
                continue;
            }
            ui32 childIndex = 4u * index + 1;
            // Children are active
            mActiveNodes[mNumActiveNodes++] = childIndex;
            mNodes[childIndex++].init();
            mActiveNodes[mNumActiveNodes++] = childIndex;
            mNodes[childIndex++].init();
            mActiveNodes[mNumActiveNodes++] = childIndex;
            mNodes[childIndex++].init();
            mActiveNodes[mNumActiveNodes++] = childIndex;
            mNodes[childIndex++].init();
        }
        else if (distance2 > GRASS_SUBDIVIDE_DISTANCES_SQ[lod - 1] * 1.1f + SQ(sDebugOptions.mGrassLodDistanceOffset) /*TODO: Non const*/) { // Don't need to check lod 0 here since it will always pass the first check
            // We can be recombined
            if (patch.mStatus == GRASS_PATCH_STATUS_VALID && !patch.didSignalRecombine()) {
                if (patch.signalParentRecombine(index, mNodes)) {
                    ui32 parentIndex = getParentIndex(index);
                    ChunkGrassPatch& parent = mNodes[parentIndex];
                    // Signal child recombination
                    ui16 childIndexFirst = 4u * parentIndex + 1;
                    ui16 childIndexLast = 4u * parentIndex + 4;
                    for (ui16 j = childIndexFirst; j <= childIndexLast; ++j) {
                        mNodes[j].mStatus = GRASS_PATCH_STATUS_WAITING_PARENT_RECOMBINE;
                    }
                    // Add parent to the active list
                    assert(!parent.isActive());
                    mActiveNodes[mNumActiveNodes++] = parentIndex;
                    parent.init(GRASS_PATCH_STATUS_RECOMBINING);

                    continue; // Do not increment to next patch
                }
            }
            else {
                // Even if we signaled for recombine, we can still mesh if our siblings aren't ready to recombine
                if (patch.isMeshDirty()) {
                    updateMeshForPatch(patch, lod, index);
                }
            }
            ++i; // Increment to next patch
        }
        else if (patch.didSignalRecombine()) {
            patch.trySignalParentNoLongerDesireRecombine(index, mNodes);
            ++i; // Increment to next patch
        }
        else {
            if (patch.isMeshDirty()) {
                updateMeshForPatch(patch, lod, index);
            }
            ++i;  // Increment to next patch
        }
    }

    // Update all crossfade, in separate table so we can deterministically bind crossfade for 5 patches at once (parent and children)
    constexpr f32 CROSSFADE_AMMOUNT = 0.05f; // TODO: Frame independent;
    for (ui32 i = 0; i < mNumCrossfading;) {
        ui16 index = mCrossfadeActiveTable[i];
        mCrossfadeTable[index] += CROSSFADE_AMMOUNT;
        if (mCrossfadeTable[index] >= 1.0f) {
            mCrossfadeActiveTable[i] = mCrossfadeActiveTable[--mNumCrossfading];
        }
        else {
            ++i;
        }
    }

    // Sort active nodes for cache efficiency
    if (needSort) {
        std::sort(mActiveNodes, mActiveNodes + mNumActiveNodes);
        // std::cout << "HAD TO SORT " << (unsigned long long)this << std::endl;
    }
}

void ChunkGrassLod::updateMeshForPatch(ChunkGrassPatch& patch, ui32 lod, ui32 patchIndex) {

    patch.mFlags &= (~GRASS_PATCH_FLAG_DIRTY_MESH);
    patch.mFlags |= GRASS_PATCH_FLAG_MESHING;
    if (!mMeshes[patchIndex]) {
        mMeshes[patchIndex] = std::make_unique<GrassBillboardMesh>();
        assert(patch.mStatus == GRASS_PATCH_STATUS_INVALID || patch.mStatus == GRASS_PATCH_STATUS_RECOMBINING);
    }
    ++mRefCount;
    mChunk.incRef();

    assert(!patch.isCrossfading() && /*!patch.isMeshing() &&*/ !patch.isMeshDirty() && patch.isActive());

    Services::Threadpool::ref().addTask([this, &patch, lod, patchIndex](ThreadPoolWorkerData*) {

        PreciseTimer timer;

        createGrassMesh(*mMeshes[patchIndex], mChunk, GRASS_PATCH_POSITIONS[patchIndex], lod);

        std::cout << "GRASS: " << lod << " " << timer.stop() << std::endl;
    }, [this, &patch, patchIndex]() {

        mMeshes[patchIndex]->finishMesh(MeshDrawMode::STATIC);
        patch.mFlags &= (~GRASS_PATCH_FLAG_MESHING);
        if (mMeshes[patchIndex]->isValid()) {
            patch.mFlags |= GRASS_PATCH_FLAG_HAS_MESH;
        }
        else {
            patch.mCrossFadeTableIndex &= (~GRASS_PATCH_FLAG_HAS_MESH);
        }

        // If recombining we wont update till next cycle
        if (patch.mStatus != GRASS_PATCH_STATUS_RECOMBINING) {
            patch.mStatus = GRASS_PATCH_STATUS_VALID;

            if (!getParent(patchIndex, mNodes).isActive()) {
                // If our parent isnt active or we arent recombining, then we can render, otherwise we will wait for parent to deactivate
                patch.mFlags |= GRASS_PATCH_FLAG_SHOULD_RENDER;
            }
        }

        // Update refcount
        --mRefCount;
        mChunk.decRef();
    });
}

ChunkGrassPatch::~ChunkGrassPatch()
{

}

void ChunkGrassPatch::destroy(ChunkGrassPatchStatus status /*= GRASS_PATCH_STATUS_INVALID*/) {
    assert(!isMeshing());
    mStatus = status;
    mFlags = 0;
    mCrossFadeTableIndex = UINT8_MAX;
}

bool ChunkGrassPatch::shouldRender() const {
    return (mFlags & GRASS_PATCH_FLAG_SHOULD_RENDER);
}

bool ChunkGrassPatch::isParentActive(ui32 myIndex, ChunkGrassPatch nodes[]) const {
    return getParent(myIndex, nodes).isActive();
}

bool ChunkGrassPatch::areChildrenDoneMeshing(ui32 myIndex, ChunkGrassPatch nodes[]) {
    int numDone = 0;
    ui16 childIndexFirst = 4u * myIndex + 1;
    for (ui16 i = 0; i < 4; ++i) {
        ui16 childIndex = childIndexFirst + i;
        if (nodes[childIndex].mStatus == GRASS_PATCH_STATUS_VALID) {
            ++numDone;
        }
    }
    return (numDone == 4);
}

void ChunkGrassPatch::initiateCrossfadeOut(ui8 crossfadeTableIndex) {
    assert(!isCrossfading());
    mFlags |= GRASS_PATCH_FLAG_CROSSFADING_OUT;
    mCrossFadeTableIndex = crossfadeTableIndex;
}

void ChunkGrassPatch::initiateCrossfadeIn(ui8 crossfadeTableIndex) {
    assert(!isCrossfading());
    mFlags |= GRASS_PATCH_FLAG_CROSSFADING_IN | GRASS_PATCH_FLAG_SHOULD_RENDER;
    mCrossFadeTableIndex = crossfadeTableIndex;
}

bool ChunkGrassPatch::signalParentRecombine(ui32 myIndex, ChunkGrassPatch nodes[]) {
    ChunkGrassPatch& parent = getParent(myIndex, nodes);
    // Make sure parent isn't in a wait state
    if (parent.mStatus >= GRASS_PATCH_STATUS_SUBDIVIDED) {
        mFlags |= GRASS_PATCH_FLAG_SIGNALLED_RECOMBINE;
        ++parent.mStatus;
        if (parent.mStatus == GRASS_PATCH_STATUS_READY_TO_RECOMBINE) {
            return true;
        }
    }
    return false;
}

void ChunkGrassPatch::trySignalParentNoLongerDesireRecombine(ui32 myIndex, ChunkGrassPatch nodes[]) {
    // Signal parent that we no longer wish to recombine
    ChunkGrassPatch& parent = getParent(myIndex, nodes);
    assert(parent.mStatus > GRASS_PATCH_STATUS_SUBDIVIDED);
    assert(didSignalRecombine());
    --parent.mStatus;
   mFlags &= (~GRASS_PATCH_FLAG_SIGNALLED_RECOMBINE);
}
