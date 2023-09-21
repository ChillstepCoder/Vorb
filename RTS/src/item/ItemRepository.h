#pragma once

#include "item/ItemDef.h"

#include "resources/IAssetRepository.h"

class TextureRepository;
struct ItemFileData;

class ItemRepository : public IAssetRepository<ItemDef> {
public:
    ASSET_REPOSITORY_COMMON_CODE(ItemRepository, ItemDef, AssetType::Item)

    ItemID getItemId(StrToken itemName) const { return (ItemID)getAssetID(itemName); }
    
private:
    AssetLoadFunc getAssetLoadFunc() override;
};

