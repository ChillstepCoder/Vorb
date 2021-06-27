#pragma once

// TODO: ItemID.h
#include "item/Item.h"

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

private:
    ItemID mGear[GEAR_SLOT_COUNT];
    // TODO: More memory efficient data structure?
    std::vector<ItemStack> mInternalStorage; // Pockets, backpack, ect
    f32 mMaxCarryWeight = 0.0f;
    f32 mTotalCarryWeight = 0.0f;
};
