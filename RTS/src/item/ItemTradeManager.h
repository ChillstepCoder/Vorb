#pragma once

#include "item/Item.h"

// Manages imports/exports for buildings, npcs, ect
// Manages item storage and audits
// Manages orders to be filled
// Can be shown via trade screen, ideally trading with an NPC is just like trading with a building.

// Once items are purchased/traded, debt is subtracted, and the items are shipped by other means,
// either instantly, or by transport

// TODO: Merge with stockpile, quartermaster???

class ItemTradeManager
{
public:
    std::map<ItemID, ui32> mItemsForSale;
    std::map<ItemID, ui32> mItemsRequested;

    std::multimap<ItemID, StoredItemStack> mItemStorage; //multimap::equal_range will get all equivalent keys
    std::map<ItemID, ui32> mStoredItemCounts;
};

