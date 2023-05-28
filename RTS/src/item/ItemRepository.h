#pragma once

#include "item/ItemDef.h"

DECL_VIO(class IOManager);

class TextureRepository;
struct ItemFileData;

class ItemRepository
{
public:
    ItemRepository(vio::IOManager& ioManager);
    ~ItemRepository();

    void loadItemFile(const vio::Path& filePath, TextureRepository& textureRepo);
    ItemID addItem(const nString& itemName, const ItemFileData& fileData);

    const ItemDef& getItem(ItemID id) const { assert(itemExists(id)); return mItems[id]; }
    const ItemDef& getItem(const nString& itemName) const;

    bool itemExists(ItemID id) const { return id < mItems.size(); }
    
private:
    vio::IOManager& mIoManager;

    std::map<nString, ItemID> mItemIdLookup;
    std::vector<ItemDef> mItems;
};

extern ItemRepository* sItemRepository;