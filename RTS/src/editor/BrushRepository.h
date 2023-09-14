#pragma once


#include "resources/IAssetRepository.h"

#include <gli/texture2d.hpp>

class BrushDef : public IAsset {
public:
    std::vector<ui8> data; // A8 alpha only
    std::unique_ptr<gli::texture2d> rs;
    ui32v2 dims;
    VGTexture texture; // TODO: Remove?
};

class BrushRepository : public IAssetRepository<BrushDef> {
public:
    ASSET_REPOSITORY_COMMON_CODE(BrushRepository, BrushDef, AssetType::Brush)

protected:
    AssetLoadFunc getAssetLoadFunc() override;
    AssetLoadFunc getAssetLoadRenderProcessFunc() override;
};

