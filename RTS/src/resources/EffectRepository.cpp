#include "stdafx.h"
#include "EffectRepository.h"

#include "resources/ParticleSystemRepository.h"

bool EffectRepository::saveAsset(AssetID assetId)
{
    const EffectDef& def = *mAssets[assetId];

    vio::Path filePath = getAssetFilePath(assetId);
    // TODO: DIALOG
    assert(!filePath.isNull());

    ryml::Tree tree;
    ryml::NodeRef root = tree.rootref();
    root |= ryml::MAP;
    root << def;

    std::stringstream ss;
    ss << tree;
    nString str = ss.str();
    return saveAssetContents(def, filePath, str.c_str(), str.size());
}

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
