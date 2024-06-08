#include "stdafx.h"
#include "SimChunkTileItemReservation.h"

#include "world/chunk/SimChunk.h"
#include "ecs/IFullECS.h"

POOLED_ALLOC_DEF_NOT_THREADSAFE(SimChunkTileItemReservation, 512, ASSERT_SIM_THREAD());

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
    return mOwnerChunk.tryPickupItemsForReservation(*this, maxCount).x;
}

i32 SimChunkTileItemReservation::tryPickupGameThread(i32 maxCount, IFullECS& ecs) {
    ASSERT_GAME_THREAD();
    i32v2 result = mOwnerChunk.tryPickupItemsForReservation(*this, maxCount);
    if (result.x) {
        // Notify ECS
        ecs.onItemPickedUp(mItemUID, result.y);
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
