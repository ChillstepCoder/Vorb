#include "stdafx.h"
#include "InventoryComponent.h"

constexpr f32 ENCUMBER_MULT = 2.0f;

InventoryComponent::InventoryComponent(f32 maxCarryWeight) :
    mMaxCarryWeight(maxCarryWeight) {

}

f32 InventoryComponent::getEncumbermentValue() const {
    const f32 diff = mTotalCarryWeight - mMaxCarryWeight;
    if (diff > 0.0f) {
        return (diff / mMaxCarryWeight) * ENCUMBER_MULT;
    }
    return 0.0f;
}

bool InventoryComponent::addOrDropItemStackToPersonalStorage(ItemStack itemStack) {
    // TODO: Handle inventory weight and overflow
    for (auto&& it : mPersonalStorage) {
        if (it.id == itemStack.id) {
            it.quantity += itemStack.quantity;
            return true;
        }
    }
    mPersonalStorage.emplace_back(itemStack);
    return true;
}

ItemStack InventoryComponent::removeItemStackFromPersonalStorage(ItemStack itemStack) {
    // TODO: Handle inventory weight and overflow
    for (size_t i = 0; i < mPersonalStorage.size(); ++i) {
        ItemStack& existing = mPersonalStorage[i];
        if (existing.id == itemStack.id) {
            if (itemStack.quantity < existing.quantity) {
                existing.quantity -= itemStack.quantity;
            }
            else {
                itemStack.quantity = existing.quantity;
                mPersonalStorage[i] = mPersonalStorage.back();
                mPersonalStorage.pop_back();
            }
            return itemStack;
        }
    }
    return ItemStack();
}

bool InventoryComponent::addOrDropItemStackToWorkingStorage(ItemStack itemStack, int workingStorageID) {

    auto&& workingStorage = mWorkingStorage[workingStorageID];
    for (size_t i = 0; i < workingStorage.size(); ++i) {
        // TODO: Stack size, dropping, ect
        if (workingStorage[i].id == itemStack.id) {
            workingStorage[i].quantity += itemStack.quantity;
            return true;
        }
    }
    workingStorage.emplace_back(itemStack);
    return true;
}

std::vector<ItemStack>& InventoryComponent::getMutableWorkingStorage(int workingStorageID) {
    return mWorkingStorage[workingStorageID];
}

void InventoryComponent::eraseWorkingStorage(int workingStorageID) {
    // TODO: Memory pool
    mWorkingStorage.erase(workingStorageID);
}
