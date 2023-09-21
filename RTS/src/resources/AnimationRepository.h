#pragma once

#include "definitions/AnimationDef.h"
#include "resources/IAssetRepository.h"
#include "rendering/model/AnimationConst.h"

class AnimationRepository : public IAssetRepository<AnimationDef>
{
public:
    ASSET_REPOSITORY_COMMON_CODE(AnimationRepository, AnimationDef, AssetType::Animation);

    bool saveAsset(AssetID assetId) override { panic("Cannot save animations yet"); }

private:
    AssetLoadFunc getAssetLoadFunc() override;

};