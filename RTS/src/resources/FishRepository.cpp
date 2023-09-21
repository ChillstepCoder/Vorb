#include "stdafx.h"
#include "FishRepository.h"

#include "item/ItemRepository.h"
#include "resources/ModelRepository.h"

AssetLoadFunc FishRepository::getAssetLoadFunc() {

    return ASSET_LOAD_LAMBDA(assetId, filePath, assetDataPtr) {
        FishDef& def = *static_cast<FishDef*>(assetDataPtr);
        YmlSerializer::readFileData(readFileToString(filePath), def);

        if (def.mItemName.isValid()) panic("FishDef {} mising item name", filePath.getString());
        def.addDependency(ItemRepository::get().getAssetHandle(def.mItemName));

        if (def.mModelName.isValid()) panic("FishDef {} mising model name", filePath.getString());
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
