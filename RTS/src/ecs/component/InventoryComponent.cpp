#include "stdafx.h"
#include "InventoryComponent.h"

#include "item/ItemRepository.h"

constexpr f32 ENCUMBER_MULT = 2.0f;

f32 InventoryComponent::getEncumbermentRatio(InventoryBagType bagType) const {
    const f32 maxCarryWeight = getMaxCarryWeight(bagType);
    const f32 diff = mBagWeights[e_cast(bagType)] - getMaxCarryWeight(bagType);
    if (diff > 0.0f) {
        return (diff / maxCarryWeight) * ENCUMBER_MULT;
    }
    return 0.0f;
}

bool InventoryComponent::addOrDropItemStack(ItemStack itemStack) {
    assert(itemStack.props.bagType != InventoryBagType::COUNT);
    const f32 weight = ItemRepository::get().getLoadedOrUnloadedAsset(itemStack.id).getWeight();
    // TODO: Handle inventory weight and overflow
    auto range = mItems.equal_range(itemStack.id);
    for (auto&& it = range.first; it != range.second; ++it) {
        if (it->second.canCombine(itemStack)) {
            it->second.count += itemStack.count;
            mBagWeights[e_cast(itemStack.props.bagType)] += weight * itemStack.count;
            return true;
        }
    }
    mItems.emplace(itemStack.id, itemStack);
    mBagWeights[e_cast(itemStack.props.bagType)] += weight * itemStack.count;
    return true;
}

int InventoryComponent::removeItemStack(ItemStack itemStack) {
    assert(itemStack.props.bagType != InventoryBagType::COUNT);
    const f32 weight = ItemRepository::get().getLoadedOrUnloadedAsset(itemStack.id).getWeight();
    auto range = mItems.equal_range(itemStack.id);
    for (auto&& it = range.first; it != range.second; ++it) {
        if (it->second.canCombine(itemStack)) {
            ItemStack& existing = it->second;
            if (existing.id == itemStack.id) {
                if (existing.count > itemStack.count) {
                    existing.count -= itemStack.count;
                    mBagWeights[e_cast(itemStack.props.bagType)] -= weight * itemStack.count;
                    return itemStack.count;
                }
                else {
                    int removedCount = existing.count;
                    mItems.erase(it);
                    mBagWeights[e_cast(itemStack.props.bagType)] -= weight * removedCount;
                    return removedCount;
                }
            }
            return true;
        }
    }
    return 0;
}

bool InventoryComponent::canCarryItemStack(ItemStack itemStack) const {
    assert(itemStack.props.bagType != InventoryBagType::COUNT);
    const f32 weight = ItemRepository::get().getLoadedOrUnloadedAsset(itemStack.id).getWeight();
    return itemStack.count * weight + mBagWeights[e_cast(itemStack.props.bagType)] <= getMaxCarryWeight(itemStack.props.bagType);
}
