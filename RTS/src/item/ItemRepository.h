#pragma once

#include "item/ItemDef.h"

#include "resources/IAssetRepository.h"

class TextureRepository;
struct ItemFileData;

class ItemRepository : public IAssetRepository<ItemDef> {
public:
    ASSET_REPOSITORY_COMMON_CODE(ItemRepository, ItemDef, AssetType::Item)

    ItemID getItemId(StrToken itemName) const { return (ItemID)getAssetID(itemName); }

    bool saveAsset(AssetID assetId) override { panic("Cannot save items yet"); }
    
    StrToken getAssetExtension() const override { return CStrToken("item"); }
    const char* const getAssetTypeDisplayName() const override { return "Item"; }

private:
    void onRegisteredAsset(AssetID id) override;
    AssetLoadFunc getAssetLoadFunc() override;
};

