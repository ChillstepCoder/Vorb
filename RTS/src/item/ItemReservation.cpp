#include "stdafx.h"

#include "item/ItemReservation.h"
#include "item/ItemStockpile.h"

#include <boost/pool/singleton_pool.hpp>

struct reservation_pool {};
using singleton_task_pool = boost::singleton_pool<reservation_pool, sizeof(ItemReservation), boost::default_user_allocator_new_delete, boost::details::pool::null_mutex, 1024u>;

ItemReservation::ItemReservation(ItemStockpile* stockpile, ItemID id, std::vector<ItemReservationTarget>&& targetSlots, bool isPromise) :
    mStockpile(stockpile), mItemID(id), mTargets(std::move(targetSlots)), mIsPromise(isPromise) {
    for (auto&& it : mTargets) {
        mRemainingQuantity += it.quantity;
    }
}

ItemReservation::~ItemReservation() {
    if (IS_SHUTTING_DOWN) {
        return;
    }
    if (mStockpile) {
        release();
    }
}

std::unique_ptr<ItemReservation> ItemReservation::splitReservation(ui16 splitQuantity) {
    assert(mStockpile);
    assert(!mIsPromise); // Unsupported
    return mStockpile->splitReservation(this, splitQuantity);
}

ui32v2 ItemReservation::getCurrentTargetWorldPosition() const {
    assert(mStockpile && mTargets.size());
    return mStockpile->getWorldPositionAtIndex(mTargets[0].index);
}

void ItemReservation::release() {
    assert(mStockpile);
    mStockpile->releaseReservation(this);
    mStockpile = nullptr;
}

bool ItemReservation::fulfillCurrentTarget(ItemStack& sourceStack) {
    return mStockpile->itemReservationFulfullCurrentTarget(this, sourceStack);
}

bool ItemReservation::cancelQuantity(ui16 quantity)
{
    assert(false);
    return false;
}

void* ItemReservation::operator new(size_t count) {
    assert(IS_GAME_THREAD());
    UNUSED(count);
    return singleton_task_pool::malloc();
}

void ItemReservation::operator delete(void* pointer, size_t size) {
    assert(IS_GAME_THREAD());
    UNUSED(size);
    return singleton_task_pool::free(pointer);
}
//
//ItemPromise::ItemPromise(ItemID itemId, ui16 minItemCount, ui16 maxItemCount, ui16 minShipmentSize, std::function<void(ItemPromise*, ui16)>&& onFinish)
//    : mItemId(itemId)
//    , mMinItemCount(minItemCount)
//    , mMaxItemCount(maxItemCount)
//    , mMinShipmentSize(minShipmentSize)
//    , mOnFinish(std::move(onFinish)) {
//    if (mMaxItemCount < mMinItemCount) {
//        mMaxItemCount = mMinItemCount;
//    }
//}
//
//ShipmentContractPtr ItemPromise::promiseShipment(ui16 quantity) {
//    assert(mItemsReady > 0);
//    mPendingShipmentQuantity += quantity;
//    assert(mPendingShipmentQuantity);
//}
//
//void ItemPromise::cancelShipment(ShipmentContractPtr&& contract) {
//    assert(mPendingShipmentQuantity >= contract->quantity);
//    mPendingShipmentQuantity -= contract->quantity;
//}
//
//ui16 ItemPromise::beginShipment(ui16 maxShipmentQuantity) {
//    assert(maxShipmentQuantity <= mPendingShipmentQuantity);
//    assert(mItemsReady > 0);
//    mShippingQuantity += maxShipmentQuantity;
//    return maxShipmentQuantity;
//}
//
//void ItemPromise::fulfillQuantity(ui16 quantity) {
//    assert(quantity <= mItemsReady);
//    mItemsReady -= quantity;
//    if (quantity >= mMinItemCount) {
//        mMinItemCount = 0;
//    }
//    else {
//        mMinItemCount -= quantity;
//    }
//    mMinItemCount -= quantity;
//    assert(quantity >= mMaxItemCount);
//    mMaxItemCount -= quantity;
//
//    mTotalFulfilled += quantity;
//
//    // We are finished
//    if (mMinItemCount == 0) {
//        assert(mTotalFulfilled >= mMinItemCount);
//        mOnFinish(this, mTotalFulfilled);
//    }
//}
