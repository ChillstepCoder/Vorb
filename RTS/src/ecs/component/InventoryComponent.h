#pragma once

#include "item/ItemStack.h"
#include <boost/container/flat_map.hpp>

enum class WorkStorageID : ui8 {
    HAULING,
};

//enum class GearSlotType {
//    BUNDLE, // Transient gear that overwrites hand slot?
//    RIGHT_HAND,
//    LEFT_HAND,
//    FEET,
//    LEGS,
//    CHEST,
//    BACK,
//    HEAD,
//    FACE,
//    RIGHT_FINGER,
//    LEFT_FINGER,
//    NECK,
//    COUNT,
//};
//constexpr ui32 GEAR_SLOT_COUNT = e_cast(GearSlotType::COUNT);
constexpr int BAG_TIERS = 4;
constexpr int MAX_BAG_TIER = BAG_TIERS - 1;
constexpr f32 BAG_CARRY_WEIGHTS[e_count(InventoryBagType)][BAG_TIERS] = {
    {100.f, 150.f, 200.f, 250.f}, // Resources
    {100.f, 150.f, 200.f, 250.f}, // Food
    {100.f, 150.f, 200.f, 250.f}, // Equipment
    {100.f, 150.f, 200.f, 250.f}, // Alchemy
    {100.f, 150.f, 200.f, 250.f}, // Valuables
    {100.f, 150.f, 200.f, 250.f}, // Misc
};

struct InventoryBag {
    int bagTier = 0;
    f32 totalCarryWeight = 0.0f;
};

class InventoryComponent {
public:

    // Between 0 an 1. When going over total carry weight, we get encumbered.
    f32 getEncumbermentValue(InventoryBagType bagType) const;
    f32 getMaxCarryWeight(InventoryBagType bagType) const { return BAG_CARRY_WEIGHTS[e_cast(bagType)][mBags[e_cast(bagType)].bagTier]; }
    f32 getTotalCarryWeight(InventoryBagType bagType) const { return mBags[e_cast(bagType)].totalCarryWeight; }

    bool addOrDropItemStack(ItemStack itemStack);
    // Returns amount removed
    int removeItemStack(ItemStack itemStack);

    bool canCarryItemStack(ItemStack itemStack) const;
    bool tryAddItemStackToWorkingStorage(ItemStack itemStack, WorkStorageID workingStorageID);
    std::vector<ItemStack>& getMutableWorkingStorage(WorkStorageID workingStorageID);
    void eraseWorkingStorage(WorkStorageID workingStorageID);

private:
    boost::container::flat_multimap<ItemID, ItemStack> mItems;
    InventoryBag mBags[e_count(InventoryBagType)];
    std::map<WorkStorageID, std::vector<ItemStack>> mWorkingStorage; // Maps inventory to work tasks and such (NPC ONLY)
};

// 88 bytes, a bit large
//SIZER(InventoryComponent)
