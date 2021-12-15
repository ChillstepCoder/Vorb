#include "stdafx.h"
#include "ChunkGrassLod.h"

#include "rendering/QuadMesh.h"
#include "world/Chunk.h"

#include "DebugRenderer.h"

const float LOG_MULT = 1.0f / (2 * log(2));

constexpr int GRASS_LOD_DETAIL[MAX_GRASS_LOD_DEPTH] = {
    0,
    1,
    2,
    4,
    8,
};

constexpr f32 GRASS_SUBDIVIDE_DISTANCES_SQ[MAX_GRASS_LOD_DEPTH] = { // sqrt(pow(WIDTH, 2) * 2) for diagonal distance widths
    FLT_MAX,
    SQ(91.0f),
    SQ(46.0f),
    SQ(23.0f),
    0.0f // Never subdivide last
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

ChunkGrassLod::ChunkGrassLod(const Chunk& chunk) : mChunk(chunk) {
    mNodes[0].mStatus = GRASS_PATCH_STATUS_INVALID;
    mActiveNodes[0] = 0;
    mNumActiveNodes = 1;

    mChunk.incRef();
}


ChunkGrassLod::~ChunkGrassLod()
{
    mChunk.decRef();
}


void ChunkGrassLod::renderDebug() {

    f32v3 pos = mChunk.getWorldPos3D();
    for (ui32 i = 0; i < mNumActiveNodes; ++i) {
        ui32 index = mActiveNodes[i];
        ChunkGrassPatch& patch = mNodes[index];
        ui32 lod = GRASS_LOD_FROM_INDEX[index];
        // TODO: Dont check this since we will never have pure root
        if (lod != 0) {
            ui32v2 posOffset = GRASS_PATCH_POSITIONS[index];
            color4 color = color4(1.0f, 1.0f, 1.0f);
            switch (patch.mStatus) {
                case GRASS_PATCH_STATUS_INVALID: color = color4(0.0f, 1.0f, 0.0f); break;
                case GRASS_PATCH_STATUS_VALID: color = color4(0.0f, 0.0f, 1.0f); break;
                case GRASS_PATCH_WAITING_SIBLINGS: color = color4(0.0f, 1.0f, 1.0f); break;
                case GRASS_PATCH_STATUS_MESHING: color = color4(1.0f, 1.0f, 0.0f); break;
                case GRASS_PATCH_STATUS_WAITING_FOR_CHILD_MESH_0: color = color4(0.3f, 0.3f, 0.3f); break;
                case GRASS_PATCH_STATUS_WAITING_FOR_CHILD_MESH_1: color = color4(0.5f, 0.5f, 0.5f); break;
                case GRASS_PATCH_STATUS_WAITING_FOR_CHILD_MESH_2: color = color4(0.7f, 0.7f, 0.7f); break;
                case GRASS_PATCH_STATUS_WAITING_FOR_CHILD_MESH_3: color = color4(0.9f, 0.9f, 0.9f); break;
                case GRASS_PATCH_STATUS_SIGNALED_RECOMBINE: color = color4(1.0f, 0.0f, 0.0f); break;
                case GRASS_PATCH_STATUS_SUBDIVIDED: color = color4(0.0f, 0.0f, 0.0f); break;
                case GRASS_PATCH_STATUS_WAITING_FOR_CHILD_RECOMBINE_0: color = color4(0.2f, 0.0f, 1.0f); break;
                case GRASS_PATCH_STATUS_WAITING_FOR_CHILD_RECOMBINE_1: color = color4(0.4f, 0.0f, 1.0f); break;
                case GRASS_PATCH_STATUS_WAITING_FOR_CHILD_RECOMBINE_2: color = color4(0.6f, 0.0f, 1.0f); break;
                case GRASS_PATCH_STATUS_READY_TO_RECOMBINE: color = color4(1.0f, 0.0f, 1.0f); break;
            }

            DebugRenderer::drawWireQuad(pos + f32v3(posOffset.x, posOffset.y, 0.0f), f32v2(LOD_DIMS[lod]), color);
        }
    }
}

void ChunkGrassLod::update(const f32v2& loadCenter)
{
    f32v2 mRelativeCenter = loadCenter - mChunk.getWorldPos();
    bool needSort = false;
    for (ui32 i = 0; i < mNumActiveNodes;) {
        ui32 index = mActiveNodes[i];
        ChunkGrassPatch& patch = mNodes[index];

        // For node I, its children are 4 * i + 1 through 4 * i + 4
        // Therefore for node I, its parent is (i - 1) / 4;
        ui32 lod = GRASS_LOD_FROM_INDEX[index];
        f32v2 centerPos = f32v2(GRASS_PATCH_POSITIONS[index]) + LOD_HALF_DIMS[lod];
            
        f32 distance2 = glm::distance2(centerPos, mRelativeCenter);
        if (distance2 < GRASS_SUBDIVIDE_DISTANCES_SQ[lod]) {
            if (patch.mStatus == GRASS_PATCH_STATUS_SIGNALED_RECOMBINE) {
                // Signal parent that we no longer wish to recombine
                ui32 parentIndex = (index - 1) / 4;
                --mNodes[parentIndex].mStatus;
                patch.mStatus = GRASS_PATCH_STATUS_INVALID; // TODO: This is wrong
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
                patch.mStatus = GRASS_PATCH_STATUS_WAITING_FOR_CHILD_MESH_0;
                ++i; // Increment to next patch
            }
            else {
                // Else we are waiting for threads
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
        else if (distance2 > GRASS_SUBDIVIDE_DISTANCES_SQ[lod - 1] * 1.1 /*TODO: Non const*/) { // Don't need to check lod 0 here since it will always pass the first check
            // We can be recombined
            if (patch.mStatus <= GRASS_PATCH_STATUS_VALID) { // TODO: Valid only?
                ui32 parentIndex = (index - 1) / 4;
                ChunkGrassPatch& parent = mNodes[parentIndex];
                // Make sure parent isn't in a wait state
                if (parent.mStatus >= GRASS_PATCH_STATUS_SUBDIVIDED) {
                    patch.mStatus = GRASS_PATCH_STATUS_SIGNALED_RECOMBINE;
                    ++parent.mStatus;
                    if (parent.mStatus == GRASS_PATCH_STATUS_READY_TO_RECOMBINE) {
                        // Perform recombination
                        ui16 childIndexFirst = 4u * parentIndex + 1;
                        ui16 childIndexLast = 4u * parentIndex + 4;
                        // Remove children from the active list via linear search
                        for (ui32 j = 0; j < mNumActiveNodes;) {
                            ui16 activeNode = mActiveNodes[j];
                            // Check if it is one of the children
                            if (activeNode >= childIndexFirst && activeNode <= childIndexLast) {
                                mNodes[mActiveNodes[j]].destroy();
                                mActiveNodes[j] = mActiveNodes[--mNumActiveNodes];
                            }
                            else {
                                ++j;
                            }
                        }
                        // Add parent to the active list
                        mActiveNodes[mNumActiveNodes++] = parentIndex;
                        parent.mStatus = GRASS_PATCH_STATUS_INVALID; // TODO: WRONG

                        needSort = true;
                        continue; // Do not increment to next patch
                    }
                }
            }
            ++i; // Increment to next patch
        }
        else if (patch.mStatus == GRASS_PATCH_STATUS_SIGNALED_RECOMBINE) {
            // Signal parent that we no longer wish to recombine
            ui32 parentIndex = (index - 1) / 4;
            ChunkGrassPatch& parent = mNodes[parentIndex];
            // Make sure parent isnt in a wait state
            if (parent.mStatus > GRASS_PATCH_STATUS_SUBDIVIDED) {
                patch.mStatus = GRASS_PATCH_STATUS_INVALID; // TODO: THIS SHOULD BE VALID WHEN WE HAVE MESH
                --parent.mStatus;
            }
            ++i; // Increment to next patch
        }
        else {
            ++i;  // Increment to next patch
        }
    }
    // Sort active nodes for cache efficiency
    if (needSort) {
        std::sort(mActiveNodes, mActiveNodes + mNumActiveNodes);
        std::cout << "HAD TO SORT " << (unsigned long long)this << std::endl;
    }
}

ChunkGrassPatch::~ChunkGrassPatch()
{

}

void ChunkGrassPatch::destroy() {
    mMesh.reset();
}
