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
    MaterialID getMaterialId(StrToken materialName) const;
    MaterialHandle getMutableMaterialHandle(StrToken materialName);
    const MaterialDesc& getMaterialDesc(StrToken materialName) const;
    const MaterialDesc& getMaterialDesc(MaterialID materialId) const;

    void uploadMaterialData(); // TODO: DO this lazily as new things are added
    void bindMaterialBuffer() const;

protected:
    virtual void initInternal() override;
    AssetLoadFunc getAssetLoadFunc() override;
    AssetLoadFunc getAssetLoadRenderProcessFunc() override;
    void onRegisteredAsset(AssetID id) override;
private:

    std::vector<MaterialDesc> mMaterialDescs;
    std::vector<MaterialGpuData> mMaterialGpuData;

    std::mutex mGeneratedStorageMutex;
    std::map<StrToken, GLTexture> mGeneratedNormalTextures;
    std::map<StrToken, GLTexture> mGeneratedAOMetallicRoughnessTextures;

    std::unique_ptr<MaterialTextureGenerator> mMaterialTextureGenerator;
    GLBuffer mMaterialDataBuffer;
};

