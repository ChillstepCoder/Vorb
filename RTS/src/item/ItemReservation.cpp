#include "stdafx.h"

#include "item/ItemReservation.h"
#include "item/ItemStockpile.h"

ItemReservation::ItemReservation(ItemStockpile* stockpile, ItemStack stack) :
    mStockpile(stockpile), mReservedItemStack(stack) {

}

ItemReservation::~ItemReservation() {
    if (mStockpile) {
        release();
    }
}

std::unique_ptr<ItemReservation> ItemReservation::splitReservation(ui32 splitQuantity) {
    return mStockpile->splitReservation(this, splitQuantity);
}

void ItemReservation::release() {
    assert(mStockpile);
    mStockpile->releaseReservation(this);
    mStockpile = nullptr;
}

bool ItemReservation::fulfillQuantity(ui32 quantity) {
    assert(quantity <= mReservedItemStack.quantity);
    return mStockpile->itemReservationFulfullQuantity(this, quantity);
}