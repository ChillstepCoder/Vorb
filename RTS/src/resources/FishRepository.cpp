#include "stdafx.h"
#include "FishRepository.h"

#include "item/ItemDef.h"
#include "definitions/ModelDef.h"

void FishRepository::onRegisteredAsset(AssetID id) {
    FishDef& def = *mAssets[id];
    const vio::Path& filePath = mAssetRegistry[id].mFilePath;
    YmlSerializer::readFileData(readFileToString(filePath), def);

    if (!def.mItemRef.isValid()) panic("FishDef {} mising item name", filePath.getString());
    if (!def.mModelRef.isValid()) panic("FishDef {} mising model name", filePath.getString());
}

AssetLoadFunc FishRepository::getAssetLoadFunc() {

    return [&]ASSET_LOAD_LAMBDA(assetId, filePath, assetDataPtr) {
        FishDef& def = *static_cast<FishDef*>(assetDataPtr);

        def.reserveDependencyCount(2);
        def.addDependency(def.mItemRef.getAssetHandleBase());
        def.addDependency(def.mModelRef.getAssetHandleBase());

        LOAD_DEPENDENCIES_HELPER(def,
            FishDef& def = *static_cast<FishDef*>(assetDataPtr);
            def.mItemId = def.getDependencies()->getLoadedAsset<ItemDef>(def.mItemRef.name).getID();
            def.mModelId = def.getDependencies()->getLoadedAsset<ModelDef>(def.mModelRef.name).getID();
        );
    };
}
