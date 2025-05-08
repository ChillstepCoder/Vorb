#pragma once

#include "resources/IAssetRepository.h"
#include "definitions/rendering/Blendspace1DDef.h"

class Blendspace1DRepository : public IAssetRepository<Blendspace1DDef>
{
public:
    ASSET_REPOSITORY_COMMON_CODE(Blendspace1DRepository, Blendspace1DDef, AssetType::Blendspace1D);

    DEFAULT_ASSET_SAVE_FUNC();
    //bool saveAsset(AssetID assetId) override { panic("Cannot save blendspaces yet"); }

    StrToken getAssetExtension() const override { return CStrToken("blend1"); }
    const char* const getAssetTypeDisplayName() const override { return "Blendspace"; }

private:
    void onRegisteredAsset(AssetID id) override;
    AssetLoadFunc getAssetLoadFunc() override;

    void fixupLoadedAsset(AssetID assetId) override;
};