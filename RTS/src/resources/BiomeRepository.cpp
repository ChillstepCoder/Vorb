#include "stdafx.h"
#include "BiomeRepository.h"

AssetLoadFunc BiomeRepository::getAssetLoadFunc() {
    return[&]ASSET_LOAD_LAMBDA(assetID, filePath, assetDataPtr) {
        assert(false);
        BiomeDef& def = *static_cast<BiomeDef*>(assetDataPtr);
        return true;
    };
}

void BiomeRepository::onRegisteredAsset(AssetID id) {
    assert(id < UINT8_MAX);

    BiomeDef& def = *mAssets[id];
    YmlSerializer::readFileData(readFileToString(mAssetRegistry[id].mFilePath), def);
    if (def.uniqueId == UINT32_MAX) {
        panic("Biome {} has no id", mAssetRegistry[id].mFilePath.getString());
    }
    if (def.uniqueId >= mUniqueIDMap.size()) {
        mUniqueIDMap.resize(def.uniqueId + 1, UINT32_MAX);
    }
    if (mUniqueIDMap[def.uniqueId] != UINT32_MAX) {
        panic("Biome {} has duplicate id {}", mAssetRegistry[id].mFilePath.getString(), def.uniqueId);
    }
    mUniqueIDMap[def.uniqueId] = id;
    mLoadedAssets[id]->store(true);
}

void BiomeRepository::onAllAssetTypesRegistered() {
    // Load mapping file so we can persist biome IDs
}
