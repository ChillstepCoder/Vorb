#include "stdafx.h"
#include "TileGrassRepository.h"
#include "resources/MaterialRepository.h"

#include <Vorb/io/IOManager.h>

AssetLoadFunc TileGrassRepository::getAssetLoadFunc() {
    return ASSET_LOAD_LAMBDA(assetID, filePath, assetDataPtr) {
        TileGrassDef& def = *static_cast<TileGrassDef*>(assetDataPtr);
        if (def.mAlphaMaskTextureName.isValid()) {
            // We will not block on dependency for materials
            def.addDependency(MaterialRepository::get().getAssetHandle(def.mAlphaMaskTextureName));
        }
        else {
            // We will not block on dependency for materials
            def.addDependency(MaterialRepository::get().getAssetHandle(def.mTextureName));
        }
        return true;
    };
}

void TileGrassRepository::onRegisteredAsset(AssetID id) {
    TileGrassDef& def = *mAssets[id];
    YmlSerializer::readFileData(readFileToString(mAssetRegistry[id].mFilePath), def);
    assert(def.mAlphaMaskTextureName.isValid() || def.mTextureName.isValid());
}

void TileGrassRepository::onAllAssetTypesRegistered() {
    // Grab material IDs
    for (AssetID id = 0; id < mAssets.size(); ++id) {
        TileGrassDef& def = *mAssets[id];
        if (def.mAlphaMaskTextureName.isValid()) {
            def.mMaterialID = MaterialRepository::get().getMaterialId(def.mAlphaMaskTextureName);
            def.mUseGradientColor = true;
        }
        else {
            def.mMaterialID = MaterialRepository::get().getMaterialId(def.mTextureName);
            def.mUseGradientColor = false;
        }
    }
}
