#include "stdafx.h"
#include "AnimMachineRepository.h"

#include "resources/RigRepository.h"
#include "resources/AnimationRepository.h"

AssetLoadFunc AnimMachineRepository::getAssetLoadFunc() {
    return [&]ASSET_LOAD_LAMBDA(assetID, filePath, assetDataPtr) {

        AnimMachineDef& def = *static_cast<AnimMachineDef*>(assetDataPtr);

        ryml::Tree tree = YmlSerializer::parseFileData(readFileToString(filePath));

        AnimMachineDefFileData fileData;
        tree.crootref() >> fileData;

        if (!fileData.mRigName.isValid()) {
            panic("Anim machine file does not have a rig: {}", filePath.getString());
        }

        def.addDependency(RigRepository::get().getAssetHandle(fileData.mRigName));

        assetLoader.requestAssetLoadWithDependencies([&, fileData]ASSET_LOAD_LAMBDA(AssetID, filePath, assetDataPtr) {
            const RigDef* rig = &def.getDependencies()->getLoadedAsset<RigDef>(fileData.mRigName);
            // Hook up all ozz animation references in the animation machine
            const StrToken* animIter = &fileData.mWalkLeftName; //< Must be mWalkLeftName as it is start of the string array
            for (ui32 i = 0; i < ANIMATION_MACHINE_ANIMS_COUNT; ++i) {
                if (animIter->isValid()) {
                    // Search for corresponding animation in the rigdef
                    auto&& it = std::find(rig->mAnimations.begin(), rig->mAnimations.end(), *animIter);
                    if (it != rig->mAnimations.end()) {
                        // TODO: THIS REQUIRES ALL ANIMS BE LOADED
                        def.mAnimsArray[i] = &AnimationRepository::get().getLoadedOrUnloadedAsset(*it).mAnimation;
                    }
                    else {
                        panic("Anim machine animation {} does not exist in rig. Machine file: {}", animIter->toString(), filePath.getString());
                    }
                }
                ++animIter;
            }
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
