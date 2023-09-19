#pragma once


#include "resources/IAssetRepository.h"

#include <gli/texture2d.hpp>
#include "definitions/BrushDef.h"

class BrushRepository : public IAssetRepository<BrushDef> {
public:
    ASSET_REPOSITORY_COMMON_CODE(BrushRepository, BrushDef, AssetType::Brush)

    bool saveAsset(AssetID assetId) override { panic("Cannot save brushes yet"); }

protected:
    AssetLoadFunc getAssetLoadFunc() override;
    AssetLoadFunc getAssetLoadRenderProcessFunc() override;
    std::any getUserData(AssetID id) override;
};

