#pragma once

#include "rendering/MaterialShaderDef.h"
#include "resources/IAssetRepository.h"

class MaterialShaderRepository : public IAssetRepository<MaterialShaderDef> {
public:
    ASSET_REPOSITORY_COMMON_CODE(MaterialShaderRepository, MaterialShaderDef, AssetType::MaterialShader)

    bool saveAsset(AssetID assetId) override { panic("Cannot save MaterialsShaders yet"); }

    StrToken getAssetExtension() const override { return CStrToken("prog"); }
    const char* const getAssetTypeDisplayName() const override { return "Shader Program"; }

private:
    bool assetSourceIsDirty(AssetID assetId) override;
    void preReloadAsset(AssetID assetId) override;
    AssetLoadFunc getAssetLoadFunc() override;
    std::any getUserData(AssetID assetId) override;
};