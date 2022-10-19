#pragma once

class Camera3D;
DECL_VG(class GLProgram);

#include "data_structure/QuadtreeSettings.h"
#include "world/ChunkID.h"

// Lookup tables are generated via this
constexpr ui32 ABSOLUTE_MAX_QUADTREE_DEPTH = 5u;
constexpr ui32 ABSOLUTE_MAX_QUADTREE_NODE_COUNT = (MathUtil::intpow<ABSOLUTE_MAX_QUADTREE_DEPTH>(4) - 1) / (4 - 1);
static_assert(ABSOLUTE_MAX_QUADTREE_NODE_COUNT < UINT16_MAX);

// ORDER MATTERS
enum QuadtreePatchStatus : ui8 {
    QUADTREE_PATCH_STATUS_INVALID,
    QUADTREE_PATCH_STATUS_VALID,
    QUADTREE_PATCH_STATUS_RECOMBINING,
    QUADTREE_PATCH_STATUS_WAITING_PARENT_RECOMBINE,
    QUADTREE_PATCH_STATUS_SUBDIVIDED,
    QUADTREE_PATCH_STATUS_WAITING_FOR_CHILD_RECOMBINE_0, // One child has requested recombine
    QUADTREE_PATCH_STATUS_WAITING_FOR_CHILD_RECOMBINE_1, // Two children have requested recombine
    QUADTREE_PATCH_STATUS_WAITING_FOR_CHILD_RECOMBINE_2, // Three children have requested recombine
    QUADTREE_PATCH_STATUS_READY_TO_RECOMBINE,            // All children have finished recombine
};

enum QuadtreePatchFlags : ui8 {
    QUADTREE_PATCH_FLAG_DIRTY_MESH          = 1 << 0,
    QUADTREE_PATCH_FLAG_MESHING             = 1 << 1,
    QUADTREE_PATCH_FLAG_SIGNALLED_RECOMBINE = 1 << 2,
    QUADTREE_PATCH_FLAG_ACTIVE              = 1 << 3,
    QUADTREE_PATCH_FLAG_SHOULD_RENDER       = 1 << 4, // TODO: remove this?
    QUADTREE_PATCH_FLAG_HAS_MESH            = 1 << 5,
    QUADTREE_PATCH_FLAG_CROSSFADING_OUT     = 1 << 6,
    QUADTREE_PATCH_FLAG_CROSSFADING_IN      = 1 << 7,
    QUADTREE_PATCH_IS_CROSSFADING           = QUADTREE_PATCH_FLAG_CROSSFADING_OUT | QUADTREE_PATCH_FLAG_CROSSFADING_IN,
    QUADTREE_PATCH_CAN_RENDER               = QUADTREE_PATCH_FLAG_SHOULD_RENDER   | QUADTREE_PATCH_FLAG_HAS_MESH
};

class QuadtreePatch {
public:
    QuadtreePatch() = default;
    ~QuadtreePatch();

    void init(QuadtreePatchStatus status = QUADTREE_PATCH_STATUS_INVALID) {
        assert(!isActive());
        mStatus = status;
        mFlags = QUADTREE_PATCH_FLAG_DIRTY_MESH | QUADTREE_PATCH_FLAG_ACTIVE;
    }
    void destroy(QuadtreePatchStatus status = QUADTREE_PATCH_STATUS_INVALID);

    bool shouldRender() const;
    bool canRender() const { return (mFlags & QUADTREE_PATCH_CAN_RENDER) == QUADTREE_PATCH_CAN_RENDER; }
    bool isActive() const { return mFlags & QUADTREE_PATCH_FLAG_ACTIVE; }
    bool isMeshDirty() const { return mFlags & QUADTREE_PATCH_FLAG_DIRTY_MESH; }
    bool isMeshing() const { return mFlags & QUADTREE_PATCH_FLAG_MESHING; }
    bool isCrossfading() const { return mFlags & QUADTREE_PATCH_IS_CROSSFADING; }
    bool didSignalRecombine() const { return mFlags & QUADTREE_PATCH_FLAG_SIGNALLED_RECOMBINE; }
    bool isParentActive(ui32 myIndex, QuadtreePatch nodes[]) const;
    bool areChildrenDoneMeshing(ui32 myIndex, QuadtreePatch nodes[]);

    void initiateCrossfadeOut(ui8 crossfadeTableIndex);
    void initiateCrossfadeIn(ui8 crossfadeTableIndex);
    bool signalParentRecombine(ui32 myIndex, QuadtreePatch nodes[]);
    void trySignalParentNoLongerDesireRecombine(ui32 myIndex, QuadtreePatch nodes[]);

    ui8 mCrossFadeTableIndex = UINT8_MAX;
    ui8 mStatus = QUADTREE_PATCH_STATUS_INVALID;
    ui8 mFlags = 0;
};
static_assert(sizeof(QuadtreePatch) == 3, "Keep tiny");

template<ui32 MAX_DEPTH, ui32 TOTAL_WIDTH, ui32 NODE_COUNT>
struct QuadtreePositionTable {
    constexpr QuadtreePositionTable();

    cui32v2 data[NODE_COUNT];
};

template<ui32 MAX_DEPTH, ui32 TOTAL_WIDTH>
class FlatQuadtree
{
public:
    template<ui32 MAX_DEPTH, ui32 TOTAL_WIDTH, ui32 NODE_COUNT> friend struct QuadtreePositionTable;

    FlatQuadtree(const f32v2& worldPos, const f32 subdivideDistances[], f32& lodDistanceOffset);
    virtual ~FlatQuadtree() { };

    // === Public Methods ===
    void renderDebug(const Camera3D& camera) const;

    // TODOL lightupdate, heavyupdate, only heavy when transition to diff cell, heavy determines splitting
    void update(const f32v2& loadCenter);

    // Mark terrain as dirty at the brush position
    void onDataChanged(const f32v2& editPosition, f32 editRadius);

    const f32v2& getWorldPos() const { return mWorldPos; }

    // === Public Constants ===
    static constexpr ui32 NODE_COUNT = (MathUtil::intpow<MAX_DEPTH>(4) - 1) / (4 - 1);
    static constexpr ui32 QUADTREE_FADE_LIST_SIZE = (MathUtil::intpow<MAX_DEPTH - 1>(4) - 1) / (4 - 1);
    static constexpr ui32 HIGHEST_LOD = MAX_DEPTH - 1;
    static_assert(QUADTREE_FADE_LIST_SIZE < UINT8_MAX);
    static_assert(MAX_DEPTH > 1 && MAX_DEPTH <= ABSOLUTE_MAX_QUADTREE_DEPTH, "Below tables are generated with max of 5");
    static_assert(NODE_COUNT <= ABSOLUTE_MAX_QUADTREE_NODE_COUNT);
    static_assert(TOTAL_WIDTH >= 64);

    static constexpr cui32v2 LOD_DIMS[ABSOLUTE_MAX_QUADTREE_DEPTH] = {
        cui32v2(TOTAL_WIDTH),
        cui32v2(TOTAL_WIDTH >> 1),
        cui32v2(TOTAL_WIDTH >> 2),
        cui32v2(TOTAL_WIDTH >> 3),
        cui32v2(TOTAL_WIDTH >> 4),
    };
private:

    void updateMeshForPatch(QuadtreePatch& patch, ui32 lod, ui32 patchIndex);

protected:
    virtual void resetCrossfadeRenderForPatch(ui32 patchIndex, int crossfadeDir, f32 crossfade) {};
    virtual void updateCrossfadeRenderForPatch(ui32 patchIndex, f32 crossfade) {};

    // === Protected Methods ===
    void onMeshFinished(ui32 patchIndex, bool isMeshValid); // Called by derived class
    ChunkID getChunkIDForPatchIndex(ui32 patchIndex) const;
    HeightmapPatchID getHeightmapPatchID(ui32 patchIndex) const;
    virtual void buildMeshForPatch(QuadtreePatch& patch, ui32 lod, ui32 patchIndex) = 0;
    virtual void freeMeshForPatch(ui32 patchIndex) = 0;

    // === Protected Constants ===
public:
    static constexpr cui32v2 LOD_HALF_DIMS[ABSOLUTE_MAX_QUADTREE_DEPTH] = {
        cui32v2(TOTAL_WIDTH / 2),
        cui32v2((TOTAL_WIDTH >> 1) / 2),
        cui32v2((TOTAL_WIDTH >> 2) / 2),
        cui32v2((TOTAL_WIDTH >> 3) / 2),
        cui32v2((TOTAL_WIDTH >> 4) / 2),
    };
    static constexpr f32 LOD_RADIUS_DIMS[ABSOLUTE_MAX_QUADTREE_DEPTH] = {
        f32(MathUtil::sqrtd(MathUtil::intpow<2>(TOTAL_WIDTH / 2) * 2.0)),
        f32(MathUtil::sqrtd(MathUtil::intpow<2>((TOTAL_WIDTH >> 1) / 2) * 2.0)),
        f32(MathUtil::sqrtd(MathUtil::intpow<2>((TOTAL_WIDTH >> 2) / 2) * 2.0)),
        f32(MathUtil::sqrtd(MathUtil::intpow<2>((TOTAL_WIDTH >> 3) / 2) * 2.0)),
        f32(MathUtil::sqrtd(MathUtil::intpow<2>((TOTAL_WIDTH >> 4) / 2) * 2.0)),
    };
    static_assert(TOTAL_WIDTH >> 4 >= 2, "We need to be able to do integer division by 2");
    static constexpr cui32v2 CHILD_OFFSETS[4] = {
        cui32v2(0, 0), // Bottom left
        cui32v2(1, 0), // Bottom Right
        cui32v2(0, 1), // Top left
        cui32v2(1, 1), // Top Right
    };
    static constexpr QuadtreePositionTable<MAX_DEPTH, TOTAL_WIDTH, NODE_COUNT> PATCH_POSITIONS = QuadtreePositionTable<MAX_DEPTH, TOTAL_WIDTH, NODE_COUNT>();
protected:
    // === Protected members ===
    f32& mLodDistanceOffset; // Reference to a setting
    ui32 mNumActiveNodes = 0;
    ui16 mActiveNodes[NODE_COUNT];
    QuadtreePatch mNodes[NODE_COUNT];
    ui32 mNumCrossfading = 0;
    ui8 mCrossfadeActiveTable[QUADTREE_FADE_LIST_SIZE];
    f32 mCrossfadeTable[QUADTREE_FADE_LIST_SIZE]; // Shared crossfade values
    f32v2 mWorldPos;
    const f32* mSubdivideDistancesSq;
};

template<ui32 MAX_DEPTH, ui32 TOTAL_WIDTH>
HeightmapPatchID FlatQuadtree<MAX_DEPTH, TOTAL_WIDTH>::getHeightmapPatchID(ui32 patchIndex) const {
    f32v2 pos = f32v2(PATCH_POSITIONS.data[patchIndex].xy);
    return HeightmapPatchID(mWorldPos + pos);
}

template<ui32 MAX_DEPTH, ui32 TOTAL_WIDTH>
ChunkID FlatQuadtree<MAX_DEPTH, TOTAL_WIDTH>::getChunkIDForPatchIndex(ui32 patchIndex) const {
    f32v2 pos = f32v2(PATCH_POSITIONS.data[patchIndex].xy);
    return ChunkID(mWorldPos + pos);
}

template<ui32 MAX_DEPTH, ui32 TOTAL_WIDTH>
void FlatQuadtree<MAX_DEPTH, TOTAL_WIDTH>::updateMeshForPatch(QuadtreePatch& patch, ui32 lod, ui32 patchIndex) {
    // If already meshing, wait
    if (patch.isMeshing()) return;
    patch.mFlags &= (~QUADTREE_PATCH_FLAG_DIRTY_MESH);
    patch.mFlags |= QUADTREE_PATCH_FLAG_MESHING;
    buildMeshForPatch(patch, lod, patchIndex);
}

template<ui32 MAX_DEPTH, ui32 TOTAL_WIDTH>
void FlatQuadtree<MAX_DEPTH, TOTAL_WIDTH>::onMeshFinished(ui32 patchIndex, bool isMeshValid) {

    QuadtreePatch& patch = mNodes[patchIndex];
    patch.mFlags &= (~QUADTREE_PATCH_FLAG_MESHING);
    if (isMeshValid) {
        patch.mFlags |= QUADTREE_PATCH_FLAG_HAS_MESH;
    }
    else {
        patch.mFlags &= (~QUADTREE_PATCH_FLAG_HAS_MESH);
        freeMeshForPatch(patchIndex);
    }

    // If recombining we wont update till next cycle
    if (patch.mStatus != QUADTREE_PATCH_STATUS_RECOMBINING) {
        patch.mStatus = QUADTREE_PATCH_STATUS_VALID;

        if (patchIndex == 0 || !getQuadtreeParent(patchIndex, mNodes).isActive()) { // LOD 0 has no parent
            // If our parent isnt active or we arent recombining, then we can render, otherwise we will wait for parent to deactivate
            patch.mFlags |= QUADTREE_PATCH_FLAG_SHOULD_RENDER;
        }
    }
}

template<ui32 MAX_DEPTH, ui32 TOTAL_WIDTH>
FlatQuadtree<MAX_DEPTH, TOTAL_WIDTH>::FlatQuadtree(const f32v2& worldPos, const f32 subdivideDistancesSq[], f32& lodDistanceOffset) : mWorldPos(worldPos), mSubdivideDistancesSq(subdivideDistancesSq), mLodDistanceOffset(lodDistanceOffset)
{
    assert(mSubdivideDistancesSq[MAX_DEPTH - 1] == -FLT_MAX); // We should never subdivide at final distance

    mNodes[0].init();
    mActiveNodes[0] = 0;
    mNumActiveNodes = 1;
}

// Generated via (log(3 * i + 1) / (2 * log(2)))
constexpr ui32 QUADTREE_LOD_FROM_INDEX[ABSOLUTE_MAX_QUADTREE_NODE_COUNT] = {
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
static_assert(ABSOLUTE_MAX_QUADTREE_DEPTH == 5);

template<ui32 MAX_DEPTH, ui32 TOTAL_WIDTH>
void FlatQuadtree<MAX_DEPTH, TOTAL_WIDTH>::onDataChanged(const f32v2& editPosition, f32 editRadius) {
    constexpr ui32 DEBUG_DURATION = 100;
    const f32v2 dims = f32v2(LOD_DIMS[0].xy);
    const f32v2 halfDims = dims * 0.5f;
    const f32v2 offsetFromCenter = editPosition - (mWorldPos + halfDims);
    if (abs(offsetFromCenter.x) < halfDims.x + editRadius && abs(offsetFromCenter.y) < halfDims.y + editRadius) {
        // This quadtree is touched, mark each intersecting child as dirty
        for (ui32 i = 0; i < mNumActiveNodes; ++i) {
            ui32 nodeIndex = mActiveNodes[i];
            QuadtreePatch& patch = mNodes[nodeIndex];
            ui32 lod = QUADTREE_LOD_FROM_INDEX[nodeIndex];
            const f32v2 patchDims = f32v2(LOD_DIMS[lod].xy);
            const f32v2 halfPatchDims = patchDims * 0.5f;
            const f32v2 patchOffsetFromCenter = editPosition - (mWorldPos + halfPatchDims + f32v2(PATCH_POSITIONS.data[nodeIndex].xy));
            if (abs(patchOffsetFromCenter.x) < halfPatchDims.x + editRadius && abs(patchOffsetFromCenter.y) < halfPatchDims.y + editRadius) {
                patch.mFlags |= QUADTREE_PATCH_FLAG_DIRTY_MESH;
                //DebugRenderer::drawWireQuad(mWorldPos + f32v2( PATCH_POSITIONS.data[nodeIndex].xy), patchDims, color4(0.2f, 1.0f, 0.2f, 1.0f), DEBUG_DURATION);
            }
        }
       // DebugRenderer::drawWireQuad(mWorldPos, dims, color4(0.0f, 1.0f, 0.0f, 1.0f), DEBUG_DURATION);
    }
}


// === Utilities ===
inline ui32 getQuadtreeParentIndex(ui32 index) {
    assert(index != 0);
    return (index - 1u) / 4u;
}
inline QuadtreePatch& getQuadtreeParent(ui32 index, QuadtreePatch nodes[]) {
    assert(index != 0);
    return nodes[(index - 1u) / 4u];
}
inline constexpr ui16 getQuadtreeChildIndexFirst(ui16 index) { return 4u * index + 1; }
inline constexpr ui16 getQuadtreeChildIndexLast(ui16 index) { return 4u * index + 4; }

// Precalculate the positions of each node at compile time
template<ui32 MAX_DEPTH, ui32 TOTAL_WIDTH, ui32 NODE_COUNT>
constexpr QuadtreePositionTable<MAX_DEPTH, TOTAL_WIDTH, NODE_COUNT>::QuadtreePositionTable()
{
    // STFU WARNING I KNOW WHAT IM DOING
#pragma warning( push )
#pragma warning( disable : 6386 ) // Disable buffer overrun warning
    for (ui32 i = 0; i < NODE_COUNT - 4; ++i) { // -4 just to shut up a compiler warning
        const ui16 firstChild = getQuadtreeChildIndexFirst(i);
        if (firstChild >= NODE_COUNT) break; // Lowest level has no children (stops buffer overrun)
        const ui16 lod = QUADTREE_LOD_FROM_INDEX[i];

        for (ui16 j = 0; j < 4; ++j) {
            const ui16 child = firstChild + j;
            data[child].x = data[i].x + FlatQuadtree<MAX_DEPTH, TOTAL_WIDTH>::CHILD_OFFSETS[j].x * FlatQuadtree<MAX_DEPTH, TOTAL_WIDTH>::LOD_HALF_DIMS[lod].x;
            data[child].y = data[i].y + FlatQuadtree<MAX_DEPTH, TOTAL_WIDTH>::CHILD_OFFSETS[j].y * FlatQuadtree<MAX_DEPTH, TOTAL_WIDTH>::LOD_HALF_DIMS[lod].y;
        }
    }
#pragma warning( pop )
}
