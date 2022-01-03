#pragma once

class GrassBillboardMesh;
class Chunk;
class Camera3D;
DECL_VG(class GLProgram);

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
constexpr int QUADTREE_FADE_LIST_SIZE = (intpow<MAX_GRASS_LOD_DEPTH - 1>(4) - 1) / (4 - 1);
static_assert(QUADTREE_FADE_LIST_SIZE < UINT8_MAX);
static_assert(MAX_GRASS_LOD_DEPTH > 1);

// ORDER MATTERS
enum ChunkGrassPatchStatus : ui8 {
    GRASS_PATCH_STATUS_INVALID,
    GRASS_PATCH_STATUS_VALID,
    GRASS_PATCH_STATUS_RECOMBINING,
    GRASS_PATCH_STATUS_WAITING_PARENT_RECOMBINE,
    GRASS_PATCH_STATUS_SUBDIVIDED,
    GRASS_PATCH_STATUS_WAITING_FOR_CHILD_RECOMBINE_0, // One child has requested recombine
    GRASS_PATCH_STATUS_WAITING_FOR_CHILD_RECOMBINE_1, // Two children have requested recombine
    GRASS_PATCH_STATUS_WAITING_FOR_CHILD_RECOMBINE_2, // Three children have requested recombine
    GRASS_PATCH_STATUS_READY_TO_RECOMBINE, // All children have finished recombine
};

enum ChunkGrassPatchFlags : ui8 {
    GRASS_PATCH_FLAG_DIRTY_MESH = 1 << 0,
    GRASS_PATCH_FLAG_MESHING = 1 << 1,
    GRASS_PATCH_FLAG_SIGNALLED_RECOMBINE = 1 << 2,
    GRASS_PATCH_FLAG_ACTIVE = 1 << 3,
    GRASS_PATCH_FLAG_SHOULD_RENDER = 1 << 4,
    GRASS_PATCH_FLAG_HAS_MESH      = 1 << 5,
    GRASS_PATCH_FLAG_CROSSFADING_OUT = 1 << 6,
    GRASS_PATCH_FLAG_CROSSFADING_IN = 1 << 7,
    GRASS_PATCH_IS_CROSSFADING = GRASS_PATCH_FLAG_CROSSFADING_OUT | GRASS_PATCH_FLAG_CROSSFADING_IN,
    GRASS_PATCH_CAN_RENDER = GRASS_PATCH_FLAG_SHOULD_RENDER | GRASS_PATCH_FLAG_HAS_MESH
};

class ChunkGrassPatch {
public:
    ChunkGrassPatch() = default;
    ~ChunkGrassPatch();

    void init(ChunkGrassPatchStatus status = GRASS_PATCH_STATUS_INVALID) {
        assert(!isActive());
        mStatus = status;
        mFlags = GRASS_PATCH_FLAG_DIRTY_MESH | GRASS_PATCH_FLAG_ACTIVE;
    }
    void destroy(ChunkGrassPatchStatus status = GRASS_PATCH_STATUS_INVALID);

    bool shouldRender() const;
    bool canRender() const { return (mFlags & GRASS_PATCH_CAN_RENDER) == GRASS_PATCH_CAN_RENDER; }
    bool isActive() const { return mFlags & GRASS_PATCH_FLAG_ACTIVE; }
    bool isMeshDirty() const { return mFlags & GRASS_PATCH_FLAG_DIRTY_MESH; }
    bool isMeshing() const { return mFlags & GRASS_PATCH_FLAG_MESHING; }
    bool isCrossfading() const { return mFlags & GRASS_PATCH_IS_CROSSFADING; }
    bool didSignalRecombine() const { return mFlags & GRASS_PATCH_FLAG_SIGNALLED_RECOMBINE; }
    bool isParentActive(ui32 myIndex, ChunkGrassPatch nodes[]) const;
    bool areChildrenDoneMeshing(ui32 myIndex, ChunkGrassPatch nodes[]);

    void initiateCrossfadeOut(ui8 crossfadeTableIndex);
    void initiateCrossfadeIn(ui8 crossfadeTableIndex);
    bool signalParentRecombine(ui32 myIndex, ChunkGrassPatch nodes[]);
    void trySignalParentNoLongerDesireRecombine(ui32 myIndex, ChunkGrassPatch nodes[]);

    ui8 mCrossFadeTableIndex = UINT8_MAX;
    ui8 mStatus = GRASS_PATCH_STATUS_INVALID;
    ui8 mFlags = 0;
};
static_assert(sizeof(ChunkGrassPatch) == 3, "Keep tiny");

// For node I, its children are 4 * i + 1 through 4 * i + 4
// A complete quadtree of N levels has 4^N - 1)
class ChunkGrassLod
{
public:
    ChunkGrassLod(const Chunk& mChunk);
    ~ChunkGrassLod();

    void render(const Camera3D& camera, const vg::GLProgram& program);
    void renderDebug(const Camera3D& camera);

    // TODOL lightupdate, heavyupdate, only heavy when transition to diff cell, heavy determines splitting
    void update(const f32v2& loadCenter);

    ui32 getRefCount() const { return mRefCount; }

private:
    void updateMeshForPatch(ChunkGrassPatch& patch, ui32 lod, ui32 patchIndex);

    // Flat for cache coherency, no allocations, and multithreading
    const Chunk& mChunk;
    ui16 mActiveNodes[QUADTREE_SIZE];
    ui32 mNumActiveNodes = 0;
    ChunkGrassPatch mNodes[QUADTREE_SIZE];
    std::unique_ptr<GrassBillboardMesh> mMeshes[QUADTREE_SIZE];
    f32 mCrossfadeTable[QUADTREE_FADE_LIST_SIZE]; // Shared crossfade values
    ui8 mCrossfadeActiveTable[QUADTREE_FADE_LIST_SIZE];
    ui32 mNumCrossfading = 0;
    ui32 mRefCount = 0;
};