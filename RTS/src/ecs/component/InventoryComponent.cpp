#include "stdafx.h"
#include "InventoryComponent.h"

#include "item/ItemRepository.h"

constexpr f32 ENCUMBER_MULT = 2.0f;

f32 InventoryComponent::getEncumbermentValue(InventoryBagType bagType) const {
    const f32 maxCarryWeight = getMaxCarryWeight(bagType);
    const f32 diff = mBags[e_cast(bagType)].totalCarryWeight - getMaxCarryWeight(bagType);
    if (diff > 0.0f) {
        return (diff / maxCarryWeight) * ENCUMBER_MULT;
    }
    return 0.0f;
}

bool InventoryComponent::addOrDropItemStack(ItemStack itemStack) {
    assert(itemStack.bagType != InventoryBagType::COUNT);
    const f32 weight = ItemRepository::get().getLoadedOrUnloadedAsset(itemStack.id).getWeight();
    // TODO: Handle inventory weight and overflow
    auto range = mItems.equal_range(itemStack.id);
    for (auto&& it = range.first; it != range.second; ++it) {
        if (it->second.canCombine(itemStack)) {
            it->second.quantity += itemStack.quantity;
            mBags[e_cast(itemStack.bagType)].totalCarryWeight += weight * itemStack.quantity;
            return true;
        }
    }
    mItems.emplace(itemStack.id, itemStack);
    mBags[e_cast(itemStack.bagType)].totalCarryWeight += weight * itemStack.quantity;
    return true;
}

int InventoryComponent::removeItemStack(ItemStack itemStack) {
    assert(itemStack.bagType != InventoryBagType::COUNT);
    const f32 weight = ItemRepository::get().getLoadedOrUnloadedAsset(itemStack.id).getWeight();
    auto range = mItems.equal_range(itemStack.id);
    for (auto&& it = range.first; it != range.second; ++it) {
        if (it->second.canCombine(itemStack)) {
            ItemStack& existing = it->second;
            if (existing.id == itemStack.id) {
                if (existing.quantity > itemStack.quantity) {
                    existing.quantity -= itemStack.quantity;
                    mBags[e_cast(itemStack.bagType)].totalCarryWeight -= weight * itemStack.quantity;
                    return itemStack.quantity;
                }
                else {
                    int removedCount = existing.quantity;
                    mItems.erase(it);
                    mBags[e_cast(itemStack.bagType)].totalCarryWeight -= weight * removedCount;
                    return removedCount;
                }
            }
            return true;
        }
    }
    return 0;
}

bool InventoryComponent::canCarryItemStack(ItemStack itemStack) const {
    assert(itemStack.bagType != InventoryBagType::COUNT);
    const f32 weight = ItemRepository::get().getLoadedOrUnloadedAsset(itemStack.id).getWeight();
    return itemStack.quantity * weight + mBags[e_cast(itemStack.bagType)].totalCarryWeight <= getMaxCarryWeight(itemStack.bagType);
}

bool InventoryComponent::tryAddItemStackToWorkingStorage(ItemStack itemStack, WorkStorageID workingStorageID) {
    assert(itemStack.bagType != InventoryBagType::COUNT);
    const f32 weight = ItemRepository::get().getLoadedOrUnloadedAsset(itemStack.id).getWeight();

    if (!canCarryItemStack(itemStack)) {
        return false;
    }

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

std::vector<ItemStack>& InventoryComponent::getMutableWorkingStorage(WorkStorageID workingStorageID) {
    return mWorkingStorage[workingStorageID];
}

void InventoryComponent::eraseWorkingStorage(WorkStorageID workingStorageID) {
    // TODO: Memory pool
    mWorkingStorage.erase(workingStorageID);
}
