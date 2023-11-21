#include "stdafx.h"
#include "BiomeRepository.h"

AssetLoadFunc BiomeRepository::getAssetLoadFunc() {
    return[&]ASSET_LOAD_LAMBDA(assetID, filePath, assetDataPtr) {
        BiomeDef& def = *static_cast<BiomeDef*>(assetDataPtr);
        return true;
    };
}
