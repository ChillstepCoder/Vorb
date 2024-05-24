#pragma once

#include "item/Recipe.h"
#include "item/ItemStack.h"

enum class ItemReservationUpdateType {
    PartialFulfill,
    END_TYPES, // Anything >= this is an end type
    CompleteFulfill = END_TYPES,
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
typedef std::function<void(ItemReservationEndReason)> SimpleItemReservationEndFunc;

class SimpleItemReservationData {
    friend class SimpleItemReservationHandleBase<std::unique_ptr<SimpleItemReservationData>>;
    friend class SimpleItemReservationHandleBase<SimpleItemReservationData*>;
    friend class SimpleItemReservationSourceHandle;
    friend class SimpleItemReservationTargetHandle;
    friend class SimpleItemReservation;
public:
    POOLED_ALLOC_DECL();

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

    ItemID desiredItems[MAX_ITEMS_IN_SIMPLE_RESERVATION] = {INVALID_ITEM_ID, INVALID_ITEM_ID, INVALID_ITEM_ID, INVALID_ITEM_ID};
    i32 filledQuantity[MAX_ITEMS_IN_SIMPLE_RESERVATION] = { 0,0,0,0 };
    i32 desiredQuantity[MAX_ITEMS_IN_SIMPLE_RESERVATION] = { 0,0,0,0 };
    i8 numItems = 0;
    i32 totalFilled = 0;
    i32 totalDesired = 0;
    SimpleItemReservationSourceHandle* sourceHandle = nullptr;
    SimpleItemReservationTargetHandle* targetHandle = nullptr;
    SimpleItemReservationUpdateFunc targetUpdateFunction = nullptr;
    SimpleItemReservationEndFunc sourceEndFunction = nullptr;
    //std::mutex mMutex;
};

template <typename PointerType>
class SimpleItemReservationHandleBase {
public:
    i32 getFilledQuantity(ItemID id) const {
        if (!dataPtr) [[unlikely]] return 0;
        const i32 itemIndex = dataPtr->getItemIndex(id);
        if (itemIndex == -1) [[unlikely]] return 0;
        return dataPtr->filledQuantity[itemIndex];
    }
    i32 getRemainingQuantity(ItemID id) const {
        if (!dataPtr) [[unlikely]] return 0;
        const i32 itemIndex = dataPtr->getItemIndex(id);
        if (itemIndex == -1) [[unlikely]] return 0;
        return dataPtr->desiredQuantity[itemIndex] - dataPtr->filledQuantity[itemIndex];
    }
    i32 getDesiredQuantity(ItemID id) const {
        if (!dataPtr) [[unlikely]] return 0;
        const i32 itemIndex = dataPtr->getItemIndex(id);
        if (itemIndex == -1) [[unlikely]] return 0;
        return dataPtr->desiredQuantity[itemIndex];
    }
    const std::span<ItemID> getDesiredItems() const {
        if (!dataPtr) [[unlikely]] return {};
        return std::span<ItemID>(dataPtr->desiredItems, dataPtr->numItems);
    }
    const std::span<i32> getFilledQuantities() const {
        if (!dataPtr) [[unlikely]] return {};
        return std::span<i32>(dataPtr->filledQuantity, dataPtr->numItems);
    }
    const std::span<i32> getDesiredQuantities() const {
        if (!dataPtr) [[unlikely]] return {};
        return std::span<i32>(dataPtr->desiredQuantity, dataPtr->numItems);
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
