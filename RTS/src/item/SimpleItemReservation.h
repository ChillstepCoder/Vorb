#pragma once

#include "item/Recipe.h"
#include "item/ItemStack.h"

enum class ItemReservationUpdateType {
    FulfillCount,
    PromiseIncrease,
    END_TYPES, // Anything >= this is an end type
    Complete = END_TYPES,
    Cancel
};

enum class ItemReservationEndReason {
    Success,
    Cancel
};
template <typename PointerType>
class SimpleItemReservationHandleBase;
class SimpleItemReservationSourceHandle;
class SimpleItemReservationTargetHandle;
class SimpleItemReservationData;
class SimpleItemReservation;

constexpr i32 MAX_ITEMS_IN_SIMPLE_RESERVATION = MAX_ITEMS_IN_RECIPE;

typedef std::function<void(ItemReservationUpdateType, ItemID, i32/*Quantity*/)> SimpleItemReservationUpdateFunc;
typedef std::function<void(ItemReservationEndReason, SimpleItemReservationData&)> SimpleItemReservationEndFunc;

class SimpleItemReservationData {
    friend class SimpleItemReservationHandleBase<std::unique_ptr<SimpleItemReservationData>>;
    friend class SimpleItemReservationHandleBase<SimpleItemReservationData*>;
    friend class SimpleItemReservationSourceHandle;
    friend class SimpleItemReservationTargetHandle;
    friend class SimpleItemReservation;
public:
    POOLED_ALLOC_DECL();

    std::span<const ItemID> getDesiredItems() const {
        return std::span<const ItemID>(desiredItems, numItems);
    }
    std::span<const i32> getRemainingQuantities() const {
        return std::span<const i32>(remainingQuantity, numItems);
    }
    i8 getNumItems() const { return numItems; }

private:
    SimpleItemReservationData(std::span<SimpleItemStack> reservedItems, SimpleItemReservationSourceHandle* sourceHandle, SimpleItemReservationTargetHandle* targetHandle);
    void cancel();
    void onComplete();
    void invalidateHandles();
    i32 getItemIndex(ItemID id) {
        for (int i = 0; i < numItems; ++i) {
            if (desiredItems[i] == id) return i;
        }
        return -1;
    }

    ItemID desiredItems[MAX_ITEMS_IN_SIMPLE_RESERVATION] = { INVALID_ITEM_ID, INVALID_ITEM_ID, INVALID_ITEM_ID, INVALID_ITEM_ID };
    i32 remainingQuantity[MAX_ITEMS_IN_SIMPLE_RESERVATION] = { 0,0,0,0 };
    i8 numItems = 0;
    i32 totalRemaining = 0;
    SimpleItemReservationSourceHandle* sourceHandle = nullptr;
    SimpleItemReservationTargetHandle* targetHandle = nullptr;
    SimpleItemReservationUpdateFunc targetUpdateFunction = nullptr;
    SimpleItemReservationEndFunc sourceEndFunction = nullptr;
    //std::mutex mMutex;
};

template <typename PointerType>
class SimpleItemReservationHandleBase {
public:
    i32 getRemainingQuantity(ItemID id) const {
        if (!dataPtr) [[unlikely]] return 0;
        const i32 itemIndex = dataPtr->getItemIndex(id);
        if (itemIndex == -1) [[unlikely]] return 0;
        return dataPtr->remainingQuantity[itemIndex];
    }
    bool desiresItem(ItemID id) const {
        if (!dataPtr) [[unlikely]] return false;
        const i32 itemIndex = dataPtr->getItemIndex(id);
        return itemIndex != -1 && dataPtr->remainingQuantity[itemIndex] > 0;
    }
    std::span<const ItemID> getDesiredItems() const {
        if (!dataPtr) [[unlikely]] return {};
        return dataPtr->getDesiredItems();
    }
    std::span<const i32> getRemainingQuantities() const {
        if (!dataPtr) [[unlikely]] return {};
        return dataPtr->getRemainingQuantities();
    }

    bool isValid() const { return dataPtr != nullptr; }
protected:
    // Only target owns the reference
    PointerType dataPtr = nullptr;
};
// Held by the one providing the items
// Has a weak reference to the reservation data
class SimpleItemReservationSourceHandle : public SimpleItemReservationHandleBase<SimpleItemReservationData*> {
    friend class SimpleItemReservation;
    friend class SimpleItemReservationData;
public:
    SimpleItemReservationSourceHandle() = default;
    ~SimpleItemReservationSourceHandle();

    VORB_NON_COPYABLE(SimpleItemReservationSourceHandle);
    POOLED_ALLOC_DECL();

    void cancel();

    // Must not overflow, returns false if this reservation was already canceled
    bool tryFulfillQuantity(ItemID id, i32 quantity);
    void increasePromisedQuantity(ItemID id, i32 quantity);

    void bindEndFunction(SimpleItemReservationEndFunc endFunction) {
        assert(dataPtr);
        assert(!dataPtr->sourceEndFunction);
        dataPtr->sourceEndFunction = endFunction;
    }
};

typedef std::unique_ptr<SimpleItemReservationSourceHandle> SimpleItemReservationSourceHandlePtr;

// Held by the one receiving the items
// Has a strong reference to the reservation data
class SimpleItemReservationTargetHandle : public SimpleItemReservationHandleBase<std::unique_ptr<SimpleItemReservationData>> {
    friend class SimpleItemReservation;
    friend class SimpleItemReservationData;
public:
    SimpleItemReservationTargetHandle() = default;
    ~SimpleItemReservationTargetHandle();

    VORB_NON_COPYABLE(SimpleItemReservationTargetHandle);
    POOLED_ALLOC_DECL();

    void cancel();

    void bindUpdateFunction(SimpleItemReservationUpdateFunc updateFunction) {
        assert(dataPtr);
        assert(!dataPtr->targetUpdateFunction);
        dataPtr->targetUpdateFunction = updateFunction;
    }
};

typedef std::unique_ptr<SimpleItemReservationTargetHandle> SimpleItemReservationTargetHandlePtr;

struct ItemReservationPair {
    SimpleItemReservationSourceHandlePtr source;
    SimpleItemReservationTargetHandlePtr target;
};
// Creates a promise between two things to exchange items some time in the future
// Handles canceling, fulfilling, ect
class SimpleItemReservation {
public:
    static ItemReservationPair createReservation(std::span<SimpleItemStack> reservedItems);
    static ItemReservationPair createReservationForRecipe(const FillableRecipe& recipe);
};
