#pragma once
#include "IAssetRepository.h"
#include "definitions/EffectDef.h"

class EffectRepository : public IAssetRepository<EffectDef> {
public:
    ASSET_REPOSITORY_COMMON_CODE(EffectRepository, EffectDef, AssetType::Effect)

    bool saveAsset(AssetID assetId) override;

    StrToken getAssetExtension() const override { return CStrToken("effect"); }
    const char* const getAssetTypeDisplayName() const override { return "Effect"; }

protected:
    AssetLoadFunc getAssetLoadFunc() override;
};

