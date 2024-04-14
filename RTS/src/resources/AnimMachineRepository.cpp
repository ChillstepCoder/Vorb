#include "stdafx.h"
#include "AnimMachineRepository.h"

#include "definitions/RigDef.h"
#include "resources/AnimationRepository.h"

AssetLoadFunc AnimMachineRepository::getAssetLoadFunc() {
    return [&]ASSET_LOAD_LAMBDA(assetID, filePath, assetDataPtr) {

        AnimMachineDef& def = *static_cast<AnimMachineDef*>(assetDataPtr);

        YmlSerializer::readFileData(readFileToString(filePath), def);
        if (!def.rigDef.isValid()) {
            panic("AnimMachine {} has no rig", filePath.getString());
        }

        def.addDependency(def.rigDef.getAssetHandle<RigDef>());

        assetLoader.requestAssetLoadWithDependencies([]ASSET_LOAD_LAMBDA(AssetID, filePath, assetDataPtr) {
            // TODO: Set everything up
            return true;
        },
            nullptr,
            assetID,
            assetDataPtr,
            filePath,
            mLoadedAssets[assetID].get(),
            nullptr,
            def.getDependencies()
        );
        return false;
    };
}
