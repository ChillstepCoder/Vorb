#pragma once

#include "definitions/RigDef.h"
#include "resources/IAssetRepository.h"

class AnimationRepository;
DECL_VIO(class IOManager);

class RigRepository : public IAssetRepository<RigDef> {
public:
    ASSET_REPOSITORY_COMMON_CODE(RigRepository, RigDef, AssetType::Rig)

    bool saveAsset(AssetID assetId) override { panic("Cannot save rigs yet"); }

    StrToken getAssetExtension() const override { return CStrToken("rig"); }
    const char* const getAssetTypeDisplayName() const override { return "Rig"; }
protected:
    AssetLoadFunc getAssetLoadFunc() override;
};

