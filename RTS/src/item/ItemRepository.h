#pragma once

#include "item/Item.h"

DECL_VIO(class IOManager);

class TextureRepository;
class ItemRepository
{
public:
    ItemRepository(vio::IOManager& ioManager);

    void loadItemFile(const vio::Path& filePath, TextureRepository& textureRepo);

    const Item& getItem(ItemID id) const { assert(itemExists(id)); return mItems[id]; }
    const Item& getItem(const nString& itemName) const;

    bool itemExists(ItemID id) const { return id < mItems.size(); }
    
private:
    vio::IOManager& mIoManager;

    std::map<nString, ItemID> mItemIdLookup;
    std::vector<Item> mItems;
};

