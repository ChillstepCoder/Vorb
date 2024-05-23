#pragma once

#include "item/ItemStack.h"

enum class ItemReservationUpdateType {
    PartialFulfill,
    CompleteFulfill,
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

class SimpleItemReservationData {
    friend class SimpleItemReservationHandleBase<std::unique_ptr<SimpleItemReservationData>>;
    friend class SimpleItemReservationHandleBase<SimpleItemReservationData*>;
    friend class SimpleItemReservationSourceHandle;
    friend class SimpleItemReservationTargetHandle;
    friend class SimpleItemReservation;
public:
    POOLED_ALLOC_DECL();

private:
    SimpleItemReservationData(ItemID itemId, i32 desiredQuantity, SimpleItemReservationSourceHandle* sourceHandle, SimpleItemReservationTargetHandle* targetHandle);
    void cancel();
    void onComplete();
    void invalidateHandles();

    i32 filledQuantity = 0;
    i32 desiredQuantity = 0;
    ItemID desiredItem = INVALID_ITEM_ID;
    SimpleItemReservationSourceHandle* sourceHandle = nullptr;
    SimpleItemReservationTargetHandle* targetHandle = nullptr;
    std::function<void(ItemReservationUpdateType, i32)> mTargetUpdateFunction = nullptr;
    std::function<void(ItemReservationEndReason)> mSourceEndFunction = nullptr;
    //std::mutex mMutex;
};

template <typename PointerType>
class SimpleItemReservationHandleBase {
public:
    i32 getFilledQuantity() const {
        if (!dataPtr) [[unlikely]] {
            return 0;
        }
        return dataPtr->filledQuantity;
    }
    i32 getRemainingQuantity() const {
        if (!dataPtr) [[unlikely]] {
            return 0;
        }
        return dataPtr->desiredQuantity - dataPtr->filledQuantity;
    }
    i32 getDesiredQuantity() const {
        if (!dataPtr) [[unlikely]] {
            return 0;
        }
        return dataPtr->desiredQuantity;
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
    bool tryFulfillQuantity(i32 quantity);

    void bindEndFunction(std::function<void(ItemReservationEndReason)> endFunction) {
        assert(dataPtr);
        assert(!dataPtr->mSourceEndFunction);
        dataPtr->mSourceEndFunction = endFunction;
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

    void bindUpdateFunction(std::function<void(ItemReservationUpdateType, i32)> updateFunction) {
        assert(dataPtr);
        assert(!dataPtr->mTargetUpdateFunction);
        dataPtr->mTargetUpdateFunction = updateFunction;
    }
};

typedef std::unique_ptr<SimpleItemReservationTargetHandle> SimpleItemReservationTargetHandlePtr;

// Creates a promise between two things to exchange items some time in the future
// Handles canceling, fulfilling, ect
class SimpleItemReservation {
public:
    static std::pair<SimpleItemReservationSourceHandlePtr, SimpleItemReservationTargetHandlePtr> createReservation(i32 desiredQuantity, ItemID desiredItem);
};
