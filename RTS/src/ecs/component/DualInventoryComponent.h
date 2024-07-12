#pragma once

#include "item/ItemStack.h"

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

enum class InventoryComponentFlags : ui16 {
    IsSimulated = BIT(0), // If true, we are not in full mode
    TERM
};
static_assert(e_cast(InventoryComponentFlags::TERM) <= 0xffff, "Must fit in 16 bits");

// Shared between Sim and Game ECS
class DualInventoryComponent {
public:

    // Between 0 an 1. When going over total carry weight, we get encumbered.
    f32 getEncumbermentRatio(InventoryBagType bagType) const;
    f32 getMaxCarryWeight(InventoryBagType bagType) const { return BAG_CARRY_WEIGHTS[e_cast(bagType)][mBagTiers[e_cast(bagType)]]; }
    // Get combined weight of all items in the bag
    f32 getTotalBagWeight(InventoryBagType bagType) const { return mBagWeights[e_cast(bagType)]; }

    bool addItemStack(ItemStack itemStack);
    // Returns amount removed
    int removeItemStack(ItemStack itemStack);

    bool canCarryItemStack(ItemStack itemStack) const;

private:
    boost::container::flat_multimap<ItemID, ItemStack> mItems;
    f32 mBagWeights[e_count(InventoryBagType)] = {}; // Combines weight of all items
    ui8 mBagTiers[e_count(InventoryBagType)] = {};
    BitFlags<InventoryComponentFlags> mFlags;
};
static_assert(sizeof(DualInventoryComponent) == 56, "Keep small, shared by sim");

