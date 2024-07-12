#include "stdafx.h"
#include "ItemStack.h"

#include "item/ItemRepository.h"

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

void ItemStack::initInternal() {
    assert(id != INVALID_ITEM_ID);
    const ItemDef& def = ItemRepository::get().getLoadedOrUnloadedAsset(id);
    props.bagType = def.mInventoryBagType;
    // TODO: REST
}
