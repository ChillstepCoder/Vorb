#include "stdafx.h"
#include "ItemStack.h"

#include "item/ItemRepository.h"

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



ItemStack::ItemStack(ui32 itemId, ui32 count) : id(itemId), count(count) {
    initInternal();
}

ItemStack::ItemStack(ui32 itemId, ui32 count, ItemProperties props) : id(itemId), count(count), props(props) {
    initInternal();
}

ItemStack::ItemStack(ui32 itemId, ui32 count, ui16 reservedCount, ItemProperties props) : id(itemId), count(count), reservedCount(reservedCount), props(props) {
    initInternal();
}

ItemStack::ItemStack(SimpleItemStack simpleStack) {
    count = simpleStack.count;
    id = simpleStack.itemId;
    initInternal();
}

void ItemStack::init(ItemID id, ui32 count) {
    this->id = id;
    this->count = count;
    initInternal();
}

void ItemStack::init(ItemID id, ui32 count, ItemProperties props) {
    this->id = id;
    this->count = count;
    this->props = props;
}

void ItemStack::initInternal() {
    assert(id != INVALID_ITEM_ID);
    const ItemDef& def = ItemRepository::get().getLoadedOrUnloadedAsset(id);
    props.bagType = def.mInventoryBagType;
    // TODO: REST
}
