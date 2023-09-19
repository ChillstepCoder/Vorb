#pragma once

#include "resources/IAssetRepository.h"
#include "rendering/material/MaterialData.h"
#include "rendering/texture/GLTexture.h"

DECL_VIO(class IOManager);

class TextureRepository;
class MaterialTextureGenerator;

class MaterialRepository : public IAssetRepository<MaterialDef> {
    friend class TileEditorPanel;
public:
    ASSET_REPOSITORY_COMMON_CODE(MaterialRepository, MaterialDef, AssetType::Material)
    ~MaterialRepository();

    const MaterialGpuData& getMaterialGpuData(MaterialID materialId) const;
    MaterialGpuData& getMutableMaterialGpuData(MaterialID materialId);
    const MaterialGpuData& getMaterialGpuData(StrToken materialName) const;
    MaterialGpuData& getMutableMaterialGpuData(StrToken materialName);
    MaterialID getMaterialId(StrToken materialName) const { return getAssetID(materialName); }
    EditorMaterialHandle getMutableMaterialHandle(StrToken materialName);
    const MaterialDesc& getMaterialDesc(StrToken materialName) const;
    const MaterialDesc& getMaterialDesc(MaterialID materialId) const;

    void bindMaterialBuffer() const;

    bool saveAsset(AssetID assetId) override { panic("Cannot save materials yet"); }

protected:

    virtual void initInternal() override;
    AssetLoadFunc getAssetLoadFunc() override;
    void onRegisteredAsset(AssetID id) override;
    std::any getUserData(AssetID id) override;
private:

    std::vector<MaterialDesc> mMaterialDescs;
    std::vector<MaterialGpuData> mMaterialGpuData;

    std::map<StrToken, GLTexture> mGeneratedNormalTextures;
    std::map<StrToken, GLTexture> mGeneratedAOMetallicRoughnessTextures;

    std::unique_ptr<MaterialTextureGenerator> mMaterialTextureGenerator;
    GLBuffer mMaterialDataBuffer;
};

