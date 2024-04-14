#include "stdafx.h"
#include "AnimationRepository.h"

#include <fstream>
#include <ozz/base/io/archive.h>
#include <ozz/base/io/stream.h>

#include "resources/RigRepository.h"
#include "filesystem/FileSystem.h"

void AnimationRepository::onRegisteredAsset(AssetID id) {
    AnimationDef& def = *mAssets[id];
    YmlSerializer::readFileData(readFileToString(mAssetRegistry[id].mFilePath), def);
}

void AnimationRepository::onAllAssetTypesRegistered() {
    for (auto& asset : mAssetRegistry) {
        AnimationDef& def = *mAssets[asset.getId()];
        if (def.rigDef.isValid()) {
            RigDef& rigHandle = RigRepository::get().getMutableLoadedOrUnloadedAsset(def.rigDef.name);
            // TODO: ShrinkToFit pass later in onPostAllAssetTypesRegistered?
            rigHandle.mAnimationDefs.emplace_back(SoftAssetReference(AssetType::Animation, def.getName()));
        }
        else {
            LOG_WARN("Warning: Animation {} has no rig", asset.mFilePath.getString());
        }
    }
}

AssetLoadFunc AnimationRepository::getAssetLoadFunc() {
    return [&]ASSET_LOAD_LAMBDA(assetID, filePath, assetDataPtr) {
        AnimationDef& def = *static_cast<AnimationDef*>(assetDataPtr);

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
        archive >> def.animation;

        return true;
    };
}
