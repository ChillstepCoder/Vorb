#pragma once

class GrassBillboardMesh;
class Chunk;

template <unsigned int p>
int constexpr intpow(const int x)
{
    if constexpr (p == 0) return 1;
    if constexpr (p == 1) return x;

    int tmp = intpow<p / 2>(x);
    if constexpr ((p % 2) == 0) { return tmp * tmp; }
    else { return x * tmp * tmp; }
}

// TODO: test dynamic
constexpr int MAX_GRASS_LOD_DEPTH = 5;
constexpr int QUADTREE_SIZE = (intpow<MAX_GRASS_LOD_DEPTH>(4) - 1) / (4 - 1);
static_assert(MAX_GRASS_LOD_DEPTH > 1);

// ORDER MATTERS
enum ChunkGrassPatchStatus : ui8 {
    GRASS_PATCH_STATUS_INVALID,
    GRASS_PATCH_STATUS_VALID,
    GRASS_PATCH_WAITING_SIBLINGS, // Waiting for siblings to finish initializing
    GRASS_PATCH_STATUS_MESHING,   // Currently on mesher thread
    GRASS_PATCH_STATUS_WAITING_FOR_CHILD_MESH_0, // No child has finished meshing
    GRASS_PATCH_STATUS_WAITING_FOR_CHILD_MESH_1, // One child has finished meshing
    GRASS_PATCH_STATUS_WAITING_FOR_CHILD_MESH_2, // Two children have finished meshing
    GRASS_PATCH_STATUS_WAITING_FOR_CHILD_MESH_3, // Three children have finished meshing
    GRASS_PATCH_STATUS_SIGNALED_RECOMBINE,
    GRASS_PATCH_STATUS_SUBDIVIDED,
    GRASS_PATCH_STATUS_WAITING_FOR_CHILD_RECOMBINE_0, // One child has requested recombine
    GRASS_PATCH_STATUS_WAITING_FOR_CHILD_RECOMBINE_1, // Two children have requested recombine
    GRASS_PATCH_STATUS_WAITING_FOR_CHILD_RECOMBINE_2, // Three children have requested recombine
    GRASS_PATCH_STATUS_READY_TO_RECOMBINE, // All children have finished recombine
};

class ChunkGrassPatch {
public:
    ChunkGrassPatch() = default;
    ~ChunkGrassPatch();

    void init() {
        mStatus = GRASS_PATCH_STATUS_INVALID;
    }
    void destroy();

    ui8 mStatus;
    std::unique_ptr<GrassBillboardMesh> mMesh;
};

// For node I, its children are 4 * i + 1 through 4 * i + 4
// A complete quadtree of N levels has 4^N - 1)
class ChunkGrassLod
{
public:
    ChunkGrassLod(const Chunk& mChunk);
    ~ChunkGrassLod();

    void renderDebug();

    // TODOL lightupdate, heavyupdate, only heavy when transition to diff cell, heavy determines splitting
    void update(const f32v2& loadCenter);

    ui32 getRefCount() const { return mRefCount; }

private:
    // Flat for cache coherency, no allocations, and multithreading
    const Chunk& mChunk;
    ui16 mActiveNodes[QUADTREE_SIZE];
    ChunkGrassPatch mNodes[QUADTREE_SIZE];
    ui32 mNumActiveNodes = 0;
    ui32 mRefCount = 0;
};

