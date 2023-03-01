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

ItemPromise::ItemPromise(ItemID itemId, ui16 itemCount, ui16 minShipmentSize) : mPromisedItemStack{itemId, itemCount}, mMinShipmentSize(minShipmentSize) {

}

void ItemPromise::promiseShipment(ui16 maxShipmentQuantity) {
    assert(mItemsReady > 0);
    mPendingShipmentQuantity += maxShipmentQuantity;
}

void ItemPromise::cancelShipment(ui16 maxShipmentQuantity) {
    assert(mPendingShipmentQuantity >= maxShipmentQuantity);
    mPendingShipmentQuantity -= maxShipmentQuantity;
}

std::shared_ptr<ItemPromise> ItemPromise::splitPromiseForShipment(ui16 maxShipmentQuantity) {
    assert(maxShipmentQuantity <= mPendingShipmentQuantity);
    assert(mItemsReady > 0);
    assert(mItemsReady <= mPromisedItemStack.quantity);

    std::shared_ptr<ItemPromise> itemPromise = std::make_shared<ItemPromise>();
}
