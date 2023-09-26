#pragma once

#include "rendering/MaterialShaderDef.h"
#include "resources/IAssetRepository.h"

class MaterialShaderRepository : public IAssetRepository<MaterialShaderDef> {
public:
    ASSET_REPOSITORY_COMMON_CODE(MaterialShaderRepository, MaterialShaderDef, AssetType::MaterialShader)

    bool saveAsset(AssetID assetId) override { panic("Cannot save MaterialsShaders yet"); }

private:
    AssetLoadFunc getAssetLoadFunc() override;
    std::any getUserData(AssetID assetId) override;
};