#pragma once

#include "definitions/SkillDef.h"

#include "resources/IAssetRepository.h"

class AnimationRepository;
DECL_VIO(class IOManager);

class SkillRepository : public IAssetRepository<SkillDef>
{
public:
    ASSET_REPOSITORY_COMMON_CODE(SkillRepository, SkillDef, AssetType::Skill)

private:
    AssetLoadFunc getAssetLoadFunc() override;
};

