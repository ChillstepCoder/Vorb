#include "stdafx.h"
#include "ItemRepository.h"

void ItemRepository::onRegisteredAsset(AssetID id) {
    ItemDef& def = *mAssets[id];
    YmlSerializer::readFileData(readFileToString(mAssetRegistry[id].mFilePath), def);
}

AssetLoadFunc ItemRepository::getAssetLoadFunc() {
    return [&]ASSET_LOAD_LAMBDA(assetId, filePath, assetDataPtr) {
        ItemDef& def = *static_cast<ItemDef*>(assetDataPtr);

        def.reserveDependencyCount(2);
        if (def.mIconTextureRef.isValid()) {
            def.addDependency(def.mIconTextureRef.getAssetHandleBase());
        }
        if (def.mModelRef.isValid()) {
            def.addDependency(def.mModelRef.getAssetHandleBase());
        }

        LOAD_DEPENDENCIES_HELPER(def);
    };
}
