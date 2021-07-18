#pragma once

#include "item/Item.h"

DECL_VIO(class IOManager);

class ItemRepository
{
public:
    ItemRepository(vio::IOManager& ioManager);

    void loadItemFile(const vio::Path& filePath);

    const Item& getItem(ItemID id) const { return mItems[id]; }
    const Item& getItem(const nString& itemName) const;
    
private:
    vio::IOManager& mIoManager;

    std::map<nString, ItemID> mItemIdLookup;
    std::vector<Item> mItems;
};

