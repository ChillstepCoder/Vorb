#pragma once

#include "item/ItemStack.h"

enum class WorkStorageID {
    HAULING,
};

enum class GearSlotType {
    BUNDLE,
    MAIN_HAND,
    OFF_HAND,
    INNER_CHEST,
    OUTER_CHEST,
    INNER_LEGS,
    OUTER_LEGS,
    FEET,
    HEAD,
    FACE,
    FINGER,
    NECK,
    COUNT,
};
constexpr ui32 GEAR_SLOT_COUNT = enum_cast(GearSlotType::COUNT);
constexpr f32 DEFAULT_CARRY_WEIGHT = 100.0f;

class InventoryComponent {
public:
    InventoryComponent(f32 maxCarryWeight);

    // Between 0 an 1. When going over total carry weight, we get encumbered.
    f32 setMaxCarryWeight(f32 maxCarryWeight) { mMaxCarryWeight = maxCarryWeight; }
    f32 getEncumbermentValue() const;
    f32 getMaxCarryWeight() const { return mMaxCarryWeight; }
    f32 getTotalCarryWeight() const { return mTotalCarryWeight; }

    bool addOrDropItemStackToPersonalStorage(ItemStack itemStack);
    ItemStack removeItemStackFromPersonalStorage(ItemStack itemStack);
    bool addOrDropItemStackToWorkingStorage(ItemStack itemStack, int workingStorageID);
    std::vector<ItemStack>& getMutableWorkingStorage(int workingStorageID);
    void eraseWorkingStorage(int workingStorageID);

private:
    ItemID mGear[GEAR_SLOT_COUNT];
    // TODO: More memory efficient data structures?
    std::vector<ItemStack> mPersonalStorage; // Pockets, backpack, ect
    std::map<int, std::vector<ItemStack>> mWorkingStorage; // Maps inventory to work tasks and such
    f32 mMaxCarryWeight = 0.0f;
    f32 mTotalCarryWeight = 0.0f;
};
