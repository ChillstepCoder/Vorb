#pragma once

#include "ItemStack.h"

class ItemStockpile;

// TODO: notify destruction
// Record of reservation of items at a particular stockpile
class ItemReservation {
public:
    friend class ItemStockpile;

    ItemReservation(ItemStockpile* stockpile, ItemStack stack);
    ~ItemReservation();

    std::unique_ptr<ItemReservation> splitReservation(ui32 splitQuantity);
    ItemStockpile& getStockpile() { return *mStockpile; }

    // Accessors
    ItemID getItemID() { return mReservedItemStack.id; }
    ui32 getRemainingQuantity() { return mReservedItemStack.quantity; }

    // Mutators
    void release();
    bool isValid() { return mStockpile != nullptr; }
    // Return true when fully fullfilled
    bool fulfillQuantity(ui32 quantity);

private:
    ItemStockpile* mStockpile = nullptr;
    ItemStack mReservedItemStack;
};
static_assert(sizeof(ItemReservation) == 16, "Keep small");