#pragma once

#include "item/ItemConst.h"

class ItemStockpile;

enum class ItemStockpileEventType {
    Create,
    Edit,
    Destroy,
};
struct ItemStockpileEvent {
    ItemStockpile* stockPile = nullptr;
    ItemID itemAddedOrRemoved;
};
EVENT_DISPATCHER_TYPE(ItemStockpile, ItemStockpileEventType, const ItemStockpileEvent&);