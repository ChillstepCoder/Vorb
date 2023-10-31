#include "stdafx.h"
#include "EffectRepository.h"

#include "resources/ParticleSystemRepository.h"

AssetLoadFunc EffectRepository::getAssetLoadFunc()
{
    return[&]ASSET_LOAD_LAMBDA(assetID, filePath, assetDataPtr) {
        EffectDef& def = *static_cast<EffectDef*>(assetDataPtr);
        YmlSerializer::readFileData(readFileToString(filePath), def);

        if (!def.mParticleSystemName.isValid()) {
            panic("Missing particle system name (psys:) on {}", filePath.getCString());
        }
        def.addDependency(ParticleSystemRepository::get().getAssetHandle(def.mParticleSystemName));

        if (def.getDependencies()->areAllAssetsLoaded()) {
            return true;
        }
        else {
            assetLoader.requestAssetLoadWithDependencies(nullptr, [&]ASSET_LOAD_LAMBDA(assetID, filePath, assetDataPtr, userData) {
                return true;
            }, assetID,
                assetDataPtr,
                filePath,
                mLoadedAssets[assetID].get(),
                nullptr,
                def.getDependencies()
            );
        }
        return false;
    };
}
