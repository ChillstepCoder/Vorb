#include "stdafx.h"
#include "FishRepository.h"

#include "item/ItemRepository.h"
#include "resources/ModelRepository.h"

void FishRepository::onRegisteredAsset(AssetID id) {
    FishDef& def = *mAssets[id];
    const vio::Path& filePath = mAssetRegistry[id].mFilePath;
    YmlSerializer::readFileData(readFileToString(filePath), def);

    if (def.mItemName.isValid()) panic("FishDef {} mising item name", filePath.getString());
    if (def.mModelName.isValid()) panic("FishDef {} mising model name", filePath.getString());
}

AssetLoadFunc FishRepository::getAssetLoadFunc() {

    return ASSET_LOAD_LAMBDA(assetId, filePath, assetDataPtr) {
        FishDef& def = *static_cast<FishDef*>(assetDataPtr);

        def.getDependencies()->reserveCount(2);
        def.addDependency(ItemRepository::get().getAssetHandle(def.mItemName));
        def.addDependency(ModelRepository::get().getAssetHandle(def.mModelName));

        assetLoader.requestAssetLoadWithDependencies(ASSET_LOAD_LAMBDA(assetId, filePath, assetDataPtr) {
            FishDef& def = *static_cast<FishDef*>(assetDataPtr);
            def.mItemId = ItemRepository::get().getAssetID(def.mItemName);
            def.mModelId = ModelRepository::get().getAssetID(def.mModelName);
            return true;
        },  nullptr,
            assetId,
            assetDataPtr,
            filePath,
            mLoadedAssets[assetId].get(),
            nullptr,
            def.getDependencies()
        );
        
        return false;
    };
}
