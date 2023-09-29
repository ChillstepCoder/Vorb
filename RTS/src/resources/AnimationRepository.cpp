#include "stdafx.h"
#include "AnimationRepository.h"

#include <fstream>
#include <ozz/base/io/archive.h>
#include <ozz/base/io/stream.h>

AssetLoadFunc AnimationRepository::getAssetLoadFunc() {
    return [&]ASSET_LOAD_LAMBDA(assetID, filePath, assetDataPtr) {
        AnimationDef& def = *static_cast<AnimationDef*>(assetDataPtr);

        ozz::io::File file(filePath.getCString(), "rb");

        if (!file.opened()) {
            pError("Animation import failure - " + filePath.getString());
            assert(false);
        }

        ozz::io::IArchive archive(&file);
        if (!archive.TestTag<ozz::animation::Animation>()) {
            pError("Animation file is not an animation - " + filePath.getString());
            assert(false);
        }
        archive >> def.mAnimation;

        return true;
    };
}
