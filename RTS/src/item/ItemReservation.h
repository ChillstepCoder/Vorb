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
    ItemPromise(ItemID itemId, ui16 minItemCount, ui16 maxItemCount, ui16 minShipmentSize, std::function<void(ItemPromise*, ui16)>&& onFinish);

    void promiseShipment(ui16 maxShipmentQuantity);
    void cancelShipment(ui16 maxShipmentQuantity);
    ui16 beginShipment(ui16 maxShipmentQuantity);
    void addItemsReadyQuantity(ui16 quantity) { mItemsReady += quantity; }
    void fulfillQuantity(ui16 quantity);
    ui16 getMinItemsRemaining() { return mItemsReady <= mMinItemCount ? (mMinItemCount - mItemsReady) : 0; }
    ui16 getMaxItemsRemaining() { assert(mItemsReady <= mMaxItemCount); return mMaxItemCount - mItemsReady; }

    bool isReadyForShipment() const { return (mItemsReady - mPendingShipmentQuantity) >= mMinShipmentSize; }
    bool isFinished() const { return mTotalFulfilled >= mMinItemCount; }

private:
    std::function<void(ItemPromise*, ui16)> mOnFinish;
    ItemID mItemId;
    ui16 mMinItemCount; // Contract is fulfilled once we fullfill this many items
    ui16 mMaxItemCount; // Allow some degree of overflow
    ui16 mItemsReady = 0;
    ui16 mMinShipmentSize; // Minimum allowed size of a shipment
    ui16 mPendingShipmentQuantity = 0; // Quantity marked for ship
    ui16 mShippingQuantity = 0; // Quantity actively shipping
    ui16 mTotalFulfilled = 0;
};

typedef std::shared_ptr<ItemPromise> ItemPromisePtr;
typedef std::weak_ptr<ItemPromise> ItemPromiseWeakPtr;