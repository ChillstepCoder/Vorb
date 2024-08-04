#include "stdafx.h"
#include "SimChunkTileItemReservation.h"

#include "world/chunk/SimChunk.h"
#include "ecs/IFullECS.h"

POOLED_ALLOC_DEF_THREADSAFE(SimChunkTileItemReservation, 512);

SimChunkTileItemReservation::~SimChunkTileItemReservation() {
    // Release reserved count
    if (mReservedCount) {
        std::lock_guard lock(mOwnerChunk.mMutex);
        auto&& it = mOwnerChunk.mItemData.itemStacks.find(mItemID);
        if (it != mOwnerChunk.mItemData.itemStacks.end()) {
            for (TileItemStack& stack : it->second) {
                if (stack.uniqueId == mItemUID) {
                    stack.reservedCount -= mReservedCount;
                    return;
                }
            }
        }
    }
}

ChunkID SimChunkTileItemReservation::getChunkID() const {
    return mOwnerChunk.getChunkID();
}

i32 SimChunkTileItemReservation::tryPickupSimThread(i32 maxCount) {
    ASSERT_SIM_THREAD();
    // TODO: Need to verify there isn't an entity on game thread for this item?
    assert(mOwnerChunk.isSimulating()); // TODO: We probably need to allow this for edge of loaded chunks or we crash later
    return mOwnerChunk.tryPickupItemsForReservation(*this, maxCount).x;
}

i32 SimChunkTileItemReservation::tryPickupGameThread(entt::entity picker, i32 maxCount, IFullECS& ecs) {
    ASSERT_GAME_THREAD();
    i32v2 result = mOwnerChunk.tryPickupItemsForReservation(*this, maxCount);
    if (result.x) {
        // We can pick up simulated items at edge of chunk
        if (!mOwnerChunk.isSimulating()) {
            // Notify ECS
            ecs.pickupTileItem(picker, mItemUID, result.x);
        }
    }
    return result.x;
}

std::unique_ptr<SimChunkTileItemReservation> SimChunkTileItemReservation::trySplit(ui16 splitCount) {
    if (splitCount >= mReservedCount || splitCount == 0) [[unlikely]] {
        assert(false);
        return nullptr;
    }
    mReservedCount -= splitCount;
    return std::make_unique<SimChunkTileItemReservation>(mTileIndex, mItemUID, mItemID, splitCount, mOwnerChunk);
}
