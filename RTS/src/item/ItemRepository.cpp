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
        def.addDependency(def.mIconTextureRef.getAssetHandle());
        def.addDependency(def.mModelRef.getAssetHandle());

        LOAD_DEPENDENCIES_HELPER(def);
    };
}
