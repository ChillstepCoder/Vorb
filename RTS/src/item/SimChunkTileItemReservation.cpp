#include "stdafx.h"
#include "SimChunkTileItemReservation.h"

#include "world/chunk/SimChunk.h"

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

i32 SimChunkTileItemReservation::tryPickup(i32 maxCount) {
    return mOwnerChunk.tryPickupItemsForReservation(*this, maxCount);
}

std::unique_ptr<SimChunkTileItemReservation> SimChunkTileItemReservation::trySplit(ui16 splitCount) {
    if (splitCount >= mReservedCount || splitCount == 0) [[unlikely]] {
        assert(false);
        return nullptr;
    }
    mReservedCount -= splitCount;
    return std::make_unique<SimChunkTileItemReservation>(mTileIndex, mItemUID, mItemID, splitCount, mOwnerChunk);
}
