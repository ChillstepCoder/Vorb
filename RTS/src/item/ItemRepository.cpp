#include "stdafx.h"
#include "ItemRepository.h"

void ItemRepository::onRegisteredAsset(AssetID id) {
    ItemDef& def = *mAssets[id];
    YmlSerializer::readFileData(readFileToString(mAssetRegistry[id].mFilePath), def);
    // All items are loaded by default for now
    mLoadedAssets[id]->store(true);
}

AssetLoadFunc ItemRepository::getAssetLoadFunc() {
    return ASSET_LOAD_LAMBDA(assetID, filePath, assetDataPtr) {
        panic("shouldnt be possible yet");
        // TODO: Model and stuff
        return true;
    };
}
