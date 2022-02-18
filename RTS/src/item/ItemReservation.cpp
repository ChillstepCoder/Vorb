#include "stdafx.h"

#include "item/ItemReservation.h"
#include "item/ItemStockpile.h"

#include <boost/pool/singleton_pool.hpp>

struct reservation_pool {};
using singleton_task_pool = boost::singleton_pool<reservation_pool, sizeof(ItemReservation)>;

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

int totalAllocated = 0;
int max = 0;
void* ItemReservation::operator new(size_t count) {
    UNUSED(count);
    std::cout << "ALLOCATING " << ++totalAllocated << std::endl;
    if (totalAllocated > max) max = totalAllocated;
    return singleton_task_pool::malloc();
}

void ItemReservation::operator delete(void* pointer, size_t size) {
    UNUSED(size);
    std::cout << "FREEING " << --totalAllocated << std::endl;
    return singleton_task_pool::free(pointer);
}
