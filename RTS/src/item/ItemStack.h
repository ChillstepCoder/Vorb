#pragma once

#include "tile/TileHarvestable.h"

constexpr ui16 MAX_ITEM_RESERVATION_SIZE = UINT16_MAX;

enum class InventoryBagType : ui8 {
    Resources,
    Food,
    Equipment,
    Alchemy,
    Valuables,
    Misc,
    COUNT
};
SERIALIZABLE_ENUM_SAME_NAME(InventoryBagType,
    pair{ InventoryBagType::Resources, "resources"sv },
    pair{ InventoryBagType::Food, "food"sv },
    pair{ InventoryBagType::Equipment, "equipment"sv },
    pair{ InventoryBagType::Alchemy, "alchemy"sv },
    pair{ InventoryBagType::Valuables, "valuables"sv },
    pair{ InventoryBagType::Misc, "misc"sv }
);
static_assert(e_count(InventoryBagType) == 6, "Update def");
static_assert(e_count(InventoryBagType) <= 8, "Must fit in 3 bits");

enum class ItemQuality : ui8 {
    Standard,  // 0
    Uncommon,  //+1
    Rare,      //+2
    Exotic,    //+3
    Legendary, //+4 < Can only be crafted out of exotic ingredients + ace minigame
    COUNT
};
static_assert(e_count(ItemQuality) <= 8, "Must fit in 3 bits");

enum class ItemBehaviorModifier : ui8 {
    None,
    Lightweight,
    Sharp,
    Reinforced,
    Enchanted,
    Brittle,
    Fiery,
    Stale,
    Rotten,
    COUNT
};
static_assert(e_count(ItemBehaviorModifier) <= 32, "Must fit in 5 bits");

enum class ItemStackFlags : ui8 {
    Important = BIT(0),
    Stolen = BIT(1),
    TERM
};
static_assert(e_cast(ItemStackFlags::TERM) <= 0b11111, "Fit in 5 bits (See ItemStack::flags)");

// Maximum stack size is 4,294,967,295 
struct ItemStack {
    ItemStack() = default;
    ItemStack(ui32 itemId, ui32 quantity) : id(itemId), quantity(quantity) {}

    bool canCombine(const ItemStack other) const {
        constexpr auto offset = offsetof(ItemStack, id);
        return memcmp(this + offset, &other + offset, sizeof(ItemStack) - offset) == 0;
    }

    ui32 quantity = 0;
    ItemID id = INVALID_ITEM_ID;
    ItemQuality quality : 3 = ItemQuality::Standard;
    ItemBehaviorModifier behaviorModifier : 5 = ItemBehaviorModifier::None;
    InventoryBagType bagType : 3 = InventoryBagType::COUNT;
    ItemStackFlags flags : 5 = {};
    ui16 durability = UINT16_MAX;
    ui16 taskReservedCount = 0; // Amount reserved for current task

    bool isNull() const { return quantity == 0; }
};
static_assert(sizeof(ItemStack) == 12);

enum class FillableItemstackFlags : ui8 {
    Harvestable = BIT(0),
};

// No durabilities, just item ID and quantity
struct SimpleItemStack {
    ItemID itemId = INVALID_ITEM_ID;
    TileHarvestable harvestableType = TileHarvestable::None;
    BitFlags<FillableItemstackFlags> flags;
    ui32 quantity = 0;
};
static_assert(sizeof(SimpleItemStack) == 8);

// Represents a ledger of a stack of simple items that we want to fill
struct FillableSimpleItemStack {

    i32 getMaxPromiseSize() const { return (desiredQuantity - filledQuantity) - promisedQuantity; }

    ItemID itemId = INVALID_ITEM_ID;
    BitFlags<FillableItemstackFlags> flags;
    TileHarvestable harvestableType = TileHarvestable::None;
    ui32 desiredQuantity = 0; 
    ui32 filledQuantity = 0; 
    ui32 promisedQuantity = 0; // Number of items that are committed to be filled but are not yet
};
static_assert(sizeof(FillableSimpleItemStack) == 16);