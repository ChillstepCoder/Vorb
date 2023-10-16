#pragma once

#include "definitions/SkillDef.h"

#include "resources/IAssetRepository.h"

class AnimationRepository;
DECL_VIO(class IOManager);

class SkillRepository : public IAssetRepository<SkillDef>
{
public:
    ASSET_REPOSITORY_COMMON_CODE(SkillRepository, SkillDef, AssetType::Skill)

    bool saveAsset(AssetID assetId) override { panic("Cannot save skills yet"); }

    StrToken getAssetExtension() const override { return CStrToken("skill"); }

private:
    AssetLoadFunc getAssetLoadFunc() override;
};

