#include "stdafx.h"
#include "Blendspace1DRepository.h"

void Blendspace1DRepository::onRegisteredAsset(AssetID id) {
    Blendspace1DDef& def = *mAssets[id];
    YmlSerializer::readFileData(readFileToString(mAssetRegistry[id].mFilePath), def);
}

AssetLoadFunc Blendspace1DRepository::getAssetLoadFunc() {
    return[&]ASSET_LOAD_LAMBDA(assetID, filePath, assetDataPtr) {
        /* AnimationDef& def = *static_cast<AnimationDef*>(assetDataPtr);

         vio::Path srcPath = filePath.getPathReplaceExtension("animsrc");
         ozz::io::File file(srcPath.getCString(), "rb");

         if (!file.opened()) {
             pError("Animation import failure - " + filePath.getString());
             assert(false);
         }

         ozz::io::IArchive archive(&file);
         if (!archive.TestTag<ozz::animation::Animation>()) {
             pError("Animation file is not an animation - " + filePath.getString());
             assert(false);
         }
         archive >> def.animation;*/

        return true;
    };
}
