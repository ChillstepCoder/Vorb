#pragma once

#include "definitions/AnimMachineDef.h"

#include "resources/IAssetRepository.h"

DECL_VIO(class IOManager);
class RigRepository;

class AnimMachineRepository : public IAssetRepository<AnimMachineDef> {
public:
    ASSET_REPOSITORY_COMMON_CODE(AnimMachineRepository, AnimMachineDef, AssetType::AnimMachine)

    bool saveAsset(AssetID assetId) override { panic("Cannot save anim machines yet"); }
protected:
    AssetLoadFunc getAssetLoadFunc() override;
};

