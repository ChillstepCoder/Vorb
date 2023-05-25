#pragma once

#include "rendering/material/MaterialData.h"
#include "rendering/texture/GLTexture.h"

DECL_VIO(class IOManager);

class TextureRepository;
class MaterialTextureGenerator;

class MaterialRepository
{
    friend class TileEditorPanel;
public:
    MaterialRepository(vio::IOManager& ioManager);
    ~MaterialRepository();

    bool loadMaterial(const vio::Path& filePath, TextureRepository& textureRepository);

    const MaterialGpuData& getMaterialGpuData(MaterialID materialId) const;
    MaterialGpuData& getMutableMaterialGpuData(MaterialID materialId);
    const MaterialGpuData& getMaterialGpuData(const nString& materialName) const;
    MaterialGpuData& getMutableMaterialGpuData(const nString& materialName);
    MaterialID getMaterialId(const nString& materialName) const;
    MaterialHandle getMutableMaterialHandle(const nString& materialName);
    const MaterialDesc& getMaterialDesc(const nString& materialName) const;
    const MaterialDesc& getMaterialDesc(MaterialID materialId) const;

    void uploadMaterialData();
    void bindMaterialBuffer() const;

private:

    std::vector<MaterialDesc> mMaterialDescs;
    std::vector<MaterialGpuData> mMaterialGpuData;
    std::map<nString, GLTexture> mGeneratedNormalTextures;
    std::map<nString, GLTexture> mGeneratedAOMetallicRoughnessTextures;
    std::map<nString, MaterialID> mMaterialIDLookup;

    vio::IOManager& mIoManager;
    std::unique_ptr<MaterialTextureGenerator> mMaterialTextureGenerator;

    GLBuffer mMaterialDataBuffer;
};

