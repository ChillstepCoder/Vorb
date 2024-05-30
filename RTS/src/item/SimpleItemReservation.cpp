#include "stdafx.h"
#include "SimpleItemReservation.h"

POOLED_ALLOC_DEF_THREADSAFE(SimpleItemReservationData, 256);
POOLED_ALLOC_DEF_THREADSAFE(SimpleItemReservationSourceHandle, 256);
POOLED_ALLOC_DEF_THREADSAFE(SimpleItemReservationTargetHandle, 256);

SimpleItemReservationData::SimpleItemReservationData(std::span<SimpleItemStack> reservedItems, SimpleItemReservationSourceHandle* sourceHandle, SimpleItemReservationTargetHandle* targetHandle) :
    sourceHandle(sourceHandle),
    targetHandle(targetHandle) {
    for (size_t i = 0; i < reservedItems.size(); ++i) {
        desiredItems[i] = reservedItems[i].itemId;
        totalRemaining += reservedItems[i].quantity;
        remainingQuantity[i] = reservedItems[i].quantity;
    }
    numItems = reservedItems.size();
    assert(numItems);
}

void SimpleItemReservationData::cancel() {
    invalidateHandles();
    if (sourceEndFunction) {
        sourceEndFunction(ItemReservationEndReason::Cancel, *this);
        sourceEndFunction = nullptr;
    }
    if (targetUpdateFunction) {
        targetUpdateFunction(ItemReservationUpdateType::Cancel, TILE_ID_NONE, 0);
        targetUpdateFunction = nullptr;
    }
}

void SimpleItemReservationData::onComplete() {
    invalidateHandles();
    if (sourceEndFunction) {
        sourceEndFunction(ItemReservationEndReason::Success, *this);
        sourceEndFunction = nullptr;
    }
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

bool SimpleItemReservationSourceHandle::tryFulfillQuantity(ItemID id, i32 quantity) {
    if (!dataPtr) [[unlikely]] {
        return false;
    }
    const i32 index = dataPtr->getItemIndex(id);
    assert(quantity <= dataPtr->remainingQuantity[index]);
    assert(quantity > 0);

    dataPtr->remainingQuantity[index] -= quantity;
    dataPtr->totalRemaining -= quantity;

    if (dataPtr->targetUpdateFunction) {
        dataPtr->targetUpdateFunction(ItemReservationUpdateType::FulfillCount, id, quantity);
        if (dataPtr->totalRemaining <= 0) {
            dataPtr->targetUpdateFunction(ItemReservationUpdateType::Complete, id, 0);
            dataPtr->onComplete();
        }
    }
    else if (dataPtr->totalRemaining <= 0) {
        dataPtr->onComplete();
    }
    
    return true;
}

void SimpleItemReservationSourceHandle::increasePromisedQuantity(ItemID id, i32 quantity) {
    if (!dataPtr) [[unlikely]] {
        return;
    }

    const i32 index = dataPtr->getItemIndex(id);
    assert(index != -1);

    dataPtr->remainingQuantity[index] += quantity;
    dataPtr->totalRemaining += quantity;
    dataPtr->targetUpdateFunction(ItemReservationUpdateType::PromiseIncrease, id, quantity);
}

//
//void SimpleItemReservationSourceHandle::adjustPromisedQuantity(ItemID id, i32 quantity) {
//    if (!dataPtr) [[unlikely]] {
//        return;
//    }
//    assert(quantity <= getMaxQuantityToPromise(id));
//    const i32 index = dataPtr->getItemIndex(id);
//    assert(index != -1);
//    dataPtr->promisedQuantity[index] += quantity;
//    dataPtr->totalPromised += quantity;
//
//    dataPtr->targetUpdateFunction(ItemReservationUpdateType::AdjustPromise, id, quantity);
//}

SimpleItemReservationTargetHandle::~SimpleItemReservationTargetHandle() {
    cancel();
}

void SimpleItemReservationTargetHandle::cancel() {
    if (!dataPtr) [[unlikely]] {
        return;
    }
    dataPtr->cancel();
}

ItemReservationPair SimpleItemReservation::createReservation(std::span<SimpleItemStack> reservedItems) {
    ItemReservationPair rv;
    rv.source = std::make_unique<SimpleItemReservationSourceHandle>();
    rv.target = std::make_unique<SimpleItemReservationTargetHandle>();
    // std::make_unique doesn't work with private constructors
    rv.target->dataPtr = std::unique_ptr<SimpleItemReservationData>(new SimpleItemReservationData(reservedItems, rv.source.get(), rv.target.get()));
    rv.source->dataPtr = rv.target->dataPtr.get();
    return rv;
}

ItemReservationPair SimpleItemReservation::createReservationForRecipe(const FillableRecipe& recipe) {
    assert(!recipe.isFullyFilled());
    ItemReservationPair rv;
    rv.source = std::make_unique<SimpleItemReservationSourceHandle>();
    rv.target = std::make_unique<SimpleItemReservationTargetHandle>();
    // std::make_unique doesn't work with private constructors
    SimpleItemStack items[MAX_ITEMS_IN_SIMPLE_RESERVATION];
    i32 total = 0;
    for (int i = 0; i < recipe.getNumItems(); ++i) {
        const i32 required = recipe.getRemainingQuantityAtIndex(i);
        if (required > 0) {
            items[total].itemId = recipe.getRequiredItems()[i];
            items[total].quantity = required;
            ++total;
        }
    }
    assert(total);
    rv.target->dataPtr = std::unique_ptr<SimpleItemReservationData>(new SimpleItemReservationData(std::span<SimpleItemStack>(items, total), rv.source.get(), rv.target.get()));
    rv.source->dataPtr = rv.target->dataPtr.get();
    return rv;
}
