#pragma once

#include "definitions/AnimationDef.h"
#include "resources/IAssetRepository.h"
#include "rendering/model/AnimationConst.h"

class AnimationRepository : public IAssetRepository<AnimationDef>
{
public:
    ASSET_REPOSITORY_COMMON_CODE(AnimationRepository, AnimationDef, AssetType::Animation);

    bool saveAsset(AssetID assetId) override { panic("Cannot save animations yet"); }

    StrToken getAssetExtension() const override { return CStrToken("anim"); }
    const char* const getAssetTypeDisplayName() const override { return "Animation"; }

private:
    AssetLoadFunc getAssetLoadFunc() override;

};