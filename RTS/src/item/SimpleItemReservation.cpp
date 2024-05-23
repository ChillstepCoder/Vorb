#include "stdafx.h"
#include "SimpleItemReservation.h"

POOLED_ALLOC_DEF_THREADSAFE(SimpleItemReservationData, 256);
POOLED_ALLOC_DEF_THREADSAFE(SimpleItemReservationSourceHandle, 256);
POOLED_ALLOC_DEF_THREADSAFE(SimpleItemReservationTargetHandle, 256);

SimpleItemReservationData::SimpleItemReservationData(ItemID itemId, i32 desiredQuantity, SimpleItemReservationSourceHandle* sourceHandle, SimpleItemReservationTargetHandle* targetHandle) :
    desiredItem(itemId),
    desiredQuantity(desiredQuantity),
    sourceHandle(sourceHandle),
    targetHandle(targetHandle) {
}

void SimpleItemReservationData::cancel() {
    if (mSourceEndFunction) {
        mSourceEndFunction(ItemReservationEndReason::Cancel);
    }
    invalidateHandles();
}

void SimpleItemReservationData::onComplete() {
    assert(filledQuantity == desiredQuantity);
    if (mSourceEndFunction) {
        mSourceEndFunction(ItemReservationEndReason::Success);
    }
    invalidateHandles();
}

void SimpleItemReservationData::invalidateHandles() {
    assert(sourceHandle);
    assert(targetHandle);
    sourceHandle->dataPtr = nullptr;
    targetHandle->dataPtr = nullptr;
    sourceHandle = nullptr;
    targetHandle = nullptr;
}

SimpleItemReservationSourceHandle::~SimpleItemReservationSourceHandle() {
    cancel();
}

void SimpleItemReservationSourceHandle::cancel() {
    if (!dataPtr) [[unlikely]] {
        return;
    }
    dataPtr->cancel();
}

bool SimpleItemReservationSourceHandle::tryFulfillQuantity(i32 quantity) {
    if (!dataPtr) [[unlikely]] {
        return false;
    }
    assert(quantity < getRemainingQuantity());

    dataPtr->filledQuantity += quantity;
    
    if (dataPtr->filledQuantity == dataPtr->desiredQuantity) {
        if (dataPtr->mTargetUpdateFunction) {
            dataPtr->mTargetUpdateFunction(ItemReservationUpdateType::CompleteFulfill, quantity);
        }
        dataPtr->onComplete();
    }
    else if (dataPtr->mTargetUpdateFunction) {
        dataPtr->mTargetUpdateFunction(ItemReservationUpdateType::PartialFulfill, quantity);
    }
    return true;
}

SimpleItemReservationTargetHandle::~SimpleItemReservationTargetHandle() {
    cancel();
}

void SimpleItemReservationTargetHandle::cancel() {
    if (!dataPtr) [[unlikely]] {
        return;
    }
    dataPtr->cancel();
}

std::pair<SimpleItemReservationSourceHandlePtr, SimpleItemReservationTargetHandlePtr> SimpleItemReservation::createReservation(i32 desiredQuantity, ItemID desiredItem) {
    std::pair<SimpleItemReservationSourceHandlePtr, SimpleItemReservationTargetHandlePtr> rv;
    rv.first = std::make_unique<SimpleItemReservationSourceHandle>();
    rv.second = std::make_unique<SimpleItemReservationTargetHandle>();
    // std::make_unique doesn't work with private constructors
    rv.second->dataPtr = std::unique_ptr<SimpleItemReservationData>(new SimpleItemReservationData(desiredQuantity, desiredItem, rv.first.get(), rv.second.get()));
    rv.first->dataPtr = rv.second->dataPtr.get();
    return rv;
}
