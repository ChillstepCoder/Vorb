#pragma once

constexpr ui16 MAX_ITEM_RESERVATION_SIZE = 65536;

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
    Terrible,  //-3
    Poor,      //-2
    Lesser,    //-1
    Standard,  // 0
    Good,      //+1
    Quality ,  //+2
    Perfect,   //+3
    Legendary, //+4
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
    ui8 flags : 5 = 0;

    bool isNull() const { return quantity == 0; }
};
static_assert(sizeof(ItemStack) == 8);