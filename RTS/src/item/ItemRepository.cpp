#include "stdafx.h"
#include "ItemRepository.h"

AssetLoadFunc ItemRepository::getAssetLoadFunc() {
    return ASSET_LOAD_LAMBDA(assetID, filePath, assetDataPtr) {
        ItemDef& def = *static_cast<ItemDef*>(assetDataPtr);
        YmlSerializer::readFileData(readFileToString(filePath), def);
        return true;
    };
}
