#pragma once

#include "ItemStack.h"

class ItemStockpile;

struct ItemReservationTarget {
    ui16 index;
    ui16 quantity;
};

// TODO: notify destruction
// Record of reservation of items at a particular stockpile, also can be the inverse, a promise of items to be added to a location
class ItemReservation {
public:
    friend class ItemStockpile;

    ItemReservation(ItemStockpile* stockpile, ItemID id, std::vector<ItemReservationTarget>&& targetSlots, bool isPromise);
    ~ItemReservation();

    std::unique_ptr<ItemReservation> splitReservation(ui16 splitQuantity);
    ItemStockpile& getStockpile() { return *mStockpile; }

    ItemReservationTarget* tryGetCurrentTarget() { return mTargets.size() ? &mTargets.back() : nullptr; }
    ui32v2 getCurrentTargetWorldPosition() const;

    // Accessors
    ItemID getItemID() const { return mItemID; }
    bool isPromise() const { return mIsPromise; }
    bool isValid() const { return mStockpile != nullptr; }
    bool isFinished() const { return mTargets.empty(); }
    ui32 getRemainingQuantity() const { return mRemainingQuantity; }

    // Mutators
    void release();
    // Return true when fully fullfilled
    bool fulfillCurrentTarget(ItemStack& sourceStack);
    bool cancelQuantity(ui16 quantity);

    // Override allocation to use boost::singleton_pool
    static void* operator new(size_t count);
    static void operator delete(void* pointer, size_t size);

private:
    std::vector<ItemReservationTarget> mTargets; // Stack of targets, pop from back
    ItemStockpile* mStockpile = nullptr;
    ui32 mRemainingQuantity;
    ItemID mItemID;
    bool mIsPromise; // If is promise, then we are giving the stacks, otherwise we are taking
};
//static_assert(sizeof(ItemReservation) == 48, "Keep small");

// Can only promise up to MAX_ITEM_RESERVATION_SIZE
class ItemPromise {
public:
    ItemPromise(ItemID itemId, ui16 itemCount, ui16 minShipmentSize);

    void promiseShipment(ui16 maxShipmentQuantity);
    void cancelShipment(ui16 maxShipmentQuantity);
    bool readyForShipment() const { return (mItemsReady - mPendingShipmentQuantity) >= mMinShipmentSize; }
    std::shared_ptr<ItemPromise> splitPromiseForShipment(ui16 maxShipmentQuantity);

private:
    ItemStack mPromisedItemStack;
    ui16 mItemsReady;
    ui16 mMinShipmentSize;
    ui16 mPendingShipmentQuantity;
};

struct ItemPromiseHandle {
    std::weak_ptr<ItemPromise> mPromisePtr;
    ItemStack mItemData;
};
