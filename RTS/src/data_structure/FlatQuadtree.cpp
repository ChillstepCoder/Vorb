#include "stdafx.h"
#include "FlatQuadtree.h"

#include "debugging/DebugRenderer.h"

#include "rendering/renderstate/RenderState.h"

QuadtreePatch::~QuadtreePatch()
{

}

void QuadtreePatch::destroy(QuadtreePatchStatus status /*= GRASS_PATCH_STATUS_INVALID*/) {
    assert(!isMeshing());
    mStatus = status;
    mFlags = 0;
    mCrossFadeTableIndex = UINT8_MAX;
}

bool QuadtreePatch::shouldRender() const {
    return (mFlags & QUADTREE_PATCH_FLAG_SHOULD_RENDER);
}

bool QuadtreePatch::isParentActive(ui32 myIndex, QuadtreePatch nodes[]) const {
    return getQuadtreeParent(myIndex, nodes).isActive();
}

bool QuadtreePatch::areChildrenDoneMeshing(ui32 myIndex, QuadtreePatch nodes[]) {
    int numDone = 0;
    ui16 childIndexFirst = getQuadtreeChildIndexFirst(myIndex);
    for (ui16 i = 0; i < 4; ++i) {
        ui16 childIndex = childIndexFirst + i;
        if (nodes[childIndex].mStatus == QUADTREE_PATCH_STATUS_VALID) {
            ++numDone;
        }
    }
    return (numDone == 4);
}

void QuadtreePatch::initiateCrossfadeOut(ui8 crossfadeTableIndex) {
    assert(!isCrossfading());
    mFlags |= QUADTREE_PATCH_FLAG_CROSSFADING_OUT;
    mCrossFadeTableIndex = crossfadeTableIndex;
}

void QuadtreePatch::initiateCrossfadeIn(ui8 crossfadeTableIndex) {
    assert(!isCrossfading());
    mFlags |= QUADTREE_PATCH_FLAG_CROSSFADING_IN | QUADTREE_PATCH_FLAG_SHOULD_RENDER;
    mCrossFadeTableIndex = crossfadeTableIndex;
}

bool QuadtreePatch::signalParentRecombine(ui32 myIndex, QuadtreePatch nodes[]) {
    QuadtreePatch& parent = getQuadtreeParent(myIndex, nodes);
    // Make sure parent isn't in a wait state
    if (parent.mStatus >= QUADTREE_PATCH_STATUS_SUBDIVIDED) {
        mFlags |= QUADTREE_PATCH_FLAG_SIGNALLED_RECOMBINE;
        ++parent.mStatus;
        if (parent.mStatus == QUADTREE_PATCH_STATUS_READY_TO_RECOMBINE) {
            return true;
        }
    }
    return false;
}

void QuadtreePatch::trySignalParentNoLongerDesireRecombine(ui32 myIndex, QuadtreePatch nodes[]) {
    // Signal parent that we no longer wish to recombine
    QuadtreePatch& parent = getQuadtreeParent(myIndex, nodes);
    assert(parent.mStatus > QUADTREE_PATCH_STATUS_SUBDIVIDED);
    assert(didSignalRecombine());
    --parent.mStatus;
    mFlags &= (~QUADTREE_PATCH_FLAG_SIGNALLED_RECOMBINE);
}

template<ui32 MAX_DEPTH, ui32 TOTAL_WIDTH>
void FlatQuadtree<MAX_DEPTH, TOTAL_WIDTH>::getDebugQuads(std::vector<DebugWireQuadState>& outQuads) const {
    assert(IS_GAME_THREAD());

    f32v3 mPos3D(mWorldPos.x, mWorldPos.y, 0.0f);
    for (ui32 i = 0; i < mNumActiveNodes; ++i) {
        ui32 index = mActiveNodes[i];
        const QuadtreePatch& patch = mNodes[index];
        ui32 lod = QUADTREE_LOD_FROM_INDEX[index];
        // TODO: Dont check this since we will never have pure root

        color4 color = color4(1.0f, 1.0f, 1.0f);

        switch (patch.mStatus) {
            case QUADTREE_PATCH_STATUS_INVALID: color = color4(0.0f, 1.0f, 0.0f); break;
            case QUADTREE_PATCH_STATUS_VALID: color = color4(0.0f, 0.0f, 1.0f); break;
            case QUADTREE_PATCH_STATUS_RECOMBINING: color = color4(0.0f, 1.0f, 1.0f); break;
            case QUADTREE_PATCH_STATUS_WAITING_PARENT_RECOMBINE: color = color4(1.0f, 1.0f, 0.0f); break;
            case QUADTREE_PATCH_STATUS_SUBDIVIDED: color = color4(0.0f, 0.0f, 0.0f); break;
            case QUADTREE_PATCH_STATUS_WAITING_FOR_CHILD_RECOMBINE_0: color = color4(0.2f, 0.0f, 1.0f); break;
            case QUADTREE_PATCH_STATUS_WAITING_FOR_CHILD_RECOMBINE_1: color = color4(0.4f, 0.0f, 1.0f); break;
            case QUADTREE_PATCH_STATUS_WAITING_FOR_CHILD_RECOMBINE_2: color = color4(0.6f, 0.0f, 1.0f); break;
            case QUADTREE_PATCH_STATUS_READY_TO_RECOMBINE: color = color4(1.0f, 0.0f, 1.0f); break;
        }

        ui32v2 posOffset = PATCH_POSITIONS.data[index].xy;
        outQuads.emplace_back(DebugWireQuadState{ mPos3D + f32v3(posOffset.x, posOffset.y, 0.0f), f32v2((ui32v2&)LOD_DIMS[lod]), color });

        if (patch.isCrossfading()) {
            const f32v2 halfDims = f32v2((ui32v2&)LOD_DIMS[lod]) * 0.5f;
            outQuads.emplace_back(DebugWireQuadState{ mPos3D + f32v3(posOffset.x + halfDims.x, posOffset.y + halfDims.y, 0.0f), halfDims, color4(1.0f, 0.0f, 1.0f) });
        }
    }
}
template void FlatQuadtree<GRASS_QUADTREE_MAX_LOD, CHUNK_WIDTH>::getDebugQuads(std::vector<DebugWireQuadState>& outQuads) const;
template void FlatQuadtree<TERRAIN_QUADTREE_MAX_LOD, TERRAIN_QUADTREE_WIDTH>::getDebugQuads(std::vector<DebugWireQuadState>& outQuads) const;


template<ui32 MAX_DEPTH, ui32 TOTAL_WIDTH>
void FlatQuadtree<MAX_DEPTH, TOTAL_WIDTH>::update(const f32v2& loadCenter)
{
    f32v2 mRelativeCenter = loadCenter - mWorldPos;
    bool needSort = false;
    for (ui32 i = 0; i < mNumActiveNodes;) {
        ui32 index = mActiveNodes[i];
        QuadtreePatch& patch = mNodes[index];

        assert(patch.isActive());

        if (patch.isCrossfading()) {
            // This should only happen while editing terrain and moving at the same time, prevent us from destroying while meshing
            // TODO: this can cause a race condition if we re-enter a different crossfade or are meshing when parent clears crossfade
            if (patch.isMeshing()) {
                ++i;
                continue;
            }
            // when crossfading, we crossfade until we are complete
            f32 currentCrossfade = mCrossfadeTable[patch.mCrossFadeTableIndex];
            if (currentCrossfade >= 1.0f) {
                if (patch.mFlags & QUADTREE_PATCH_FLAG_CROSSFADING_OUT) {
                    if (currentCrossfade >= 1.0f) {
                        // Free mesh
                        freeMeshForPatch(index);
                        // Destroy and continue
                        if (patch.didSignalRecombine()) {
                            // We are combining into parent
                            patch.destroy(QUADTREE_PATCH_STATUS_INVALID);
                        }
                        else {
                            // We are subdividing into children
                            patch.destroy(QUADTREE_PATCH_STATUS_SUBDIVIDED);
                        }
                        mActiveNodes[i] = mActiveNodes[--mNumActiveNodes];
                        needSort = true;
                        continue;
                    }
                }
                else {
                    patch.mFlags &= (~QUADTREE_PATCH_IS_CROSSFADING);
                }
                resetCrossfadeRenderForPatch(index, 0, currentCrossfade);
            }
            else {
                updateCrossfadeRenderForPatch(index, currentCrossfade);
            }
            ++i;
            continue;
        }
        else if (patch.isMeshing()) {
            // When patches are meshing, we wait for them to complete
            ++i;
            continue;
        }
        else if (patch.mStatus == QUADTREE_PATCH_STATUS_WAITING_PARENT_RECOMBINE) {
            // Waiting on parent to mesh and stuff, do nothing
            ++i;
            continue;
        }

        // For node I, its children are 4 * i + 1 through 4 * i + 4
        // Therefore for node I, its parent is (i - 1) / 4;
        ui32 lod = QUADTREE_LOD_FROM_INDEX[index];
        assert(!patch.isCrossfading());
        // If our parent is active, we will do nothing but mesh, since the parent is either waiting on us to mesh, or is recombining us
        if (lod > 0 && getQuadtreeParent(index, mNodes).isActive()) {
            assert(patch.mStatus != QUADTREE_PATCH_STATUS_SUBDIVIDED);
            if (patch.isMeshDirty()) {
                updateMeshForPatch(patch, lod, index);
            }
            ++i;
            continue;
        }
        else if (patch.mStatus == QUADTREE_PATCH_STATUS_RECOMBINING) {
            if (patch.isMeshDirty()) {
                updateMeshForPatch(patch, lod, index);
                ++i; // Move to next
            }
            else if (!patch.isMeshing()) {
                // We can recombine
                patch.mStatus = QUADTREE_PATCH_STATUS_VALID;
                // Start crossfade
                assert(index < QUADTREE_FADE_LIST_SIZE);
                mCrossfadeTable[index] = 0.0f;
                mCrossfadeActiveTable[mNumCrossfading++] = index;
                patch.initiateCrossfadeIn(index);
                resetCrossfadeRenderForPatch(index, 1, 0.0f);
                ui16 childIndexFirst = getQuadtreeChildIndexFirst(index);
                ui16 childIndexLast = getQuadtreeChildIndexLast(index);
                // Crossfade children
                for (ui16 j = childIndexFirst; j <= childIndexLast; ++j) {
                    mNodes[j].initiateCrossfadeOut(index);
                    resetCrossfadeRenderForPatch(j, -1, 0.0f);
                }
                // Don't move to next
            }
            continue;
        }

        f32v2 centerPos = f32v2(PATCH_POSITIONS.data[index].xy) + f32v2(LOD_HALF_DIMS[lod].xy);

        f32 distance2 = glm::distance2(centerPos, mRelativeCenter);
        if (distance2 < mSubdivideDistancesSq[lod] + SQ(mLodDistanceOffset)) {
            // We want to subdivide
            if (patch.mStatus == QUADTREE_PATCH_STATUS_SUBDIVIDED) {
                // If we reach here we are still active and waiting on children, check if our
                // children are finished meshing
                if (patch.areChildrenDoneMeshing(index, mNodes)) {
                    // Start crossfade
                    assert(index < QUADTREE_FADE_LIST_SIZE);
                    mCrossfadeTable[index] = 0.0f;
                    mCrossfadeActiveTable[mNumCrossfading++] = index;
                    // Tell children they can draw
                    ui16 childIndexFirst = getQuadtreeChildIndexFirst(index);
                    for (ui16 i = 0; i < 4; ++i) {
                        const ui16 childIndex = childIndexFirst + i;
                        QuadtreePatch& child = mNodes[childIndex];
                        child.initiateCrossfadeIn(index);
                        resetCrossfadeRenderForPatch(childIndex, 1, 0.0f);
                    }
                    patch.initiateCrossfadeOut(index);
                    resetCrossfadeRenderForPatch(index, -1, 0.0f);
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
            if (patch.mStatus == QUADTREE_PATCH_STATUS_INVALID) {
                // Pop and swap
                patch.destroy();
                mActiveNodes[i] = mActiveNodes[--mNumActiveNodes];
                patch.mStatus = QUADTREE_PATCH_STATUS_SUBDIVIDED;
                needSort = true;
                // Do not increment to next patch
            }
            else if (patch.mStatus == QUADTREE_PATCH_STATUS_VALID) {
                // Subdivide, we are still active, but we are waiting for our children to finish initializing
                patch.mStatus = QUADTREE_PATCH_STATUS_SUBDIVIDED;
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
        else if (lod > 0 && distance2 > mSubdivideDistancesSq[lod - 1] * 1.1f + SQ(mLodDistanceOffset) /*TODO: Non const*/) {
            // We can be recombined
            if (patch.mStatus == QUADTREE_PATCH_STATUS_VALID && !patch.didSignalRecombine()) {
                if (patch.signalParentRecombine(index, mNodes)) {
                    ui32 parentIndex = getQuadtreeParentIndex(index);
                    QuadtreePatch& parent = mNodes[parentIndex];
                    // Signal child recombination
                    ui16 childIndexFirst = getQuadtreeChildIndexFirst(parentIndex);
                    ui16 childIndexLast = getQuadtreeChildIndexLast(parentIndex);
                    for (ui16 j = childIndexFirst; j <= childIndexLast; ++j) {
                        mNodes[j].mStatus = QUADTREE_PATCH_STATUS_WAITING_PARENT_RECOMBINE;
                    }
                    // Add parent to the active list
                    assert(!parent.isActive());
                    mActiveNodes[mNumActiveNodes++] = parentIndex;
                    parent.init(QUADTREE_PATCH_STATUS_RECOMBINING);

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
    constexpr f32 CROSSFADE_AMMOUNT = 0.05f;
    for (ui32 i = 0; i < mNumCrossfading;) {
        ui16 crossfadeIndex = mCrossfadeActiveTable[i];
        mCrossfadeTable[crossfadeIndex] += CROSSFADE_AMMOUNT/* * deltaTime*/;
        if (mCrossfadeTable[crossfadeIndex] >= 1.0f) {
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
template void FlatQuadtree<GRASS_QUADTREE_MAX_LOD, CHUNK_WIDTH>::update(const f32v2&);
template void FlatQuadtree<TERRAIN_QUADTREE_MAX_LOD, TERRAIN_QUADTREE_WIDTH>::update(const f32v2&);
