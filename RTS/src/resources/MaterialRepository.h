#pragma once

#include "rendering/material/MaterialData.h"
#include "rendering/texture/GLTexture.h"

DECL_VIO(class IOManager);

class TextureRepository;
class NormalMapGenerator;

class MaterialRepository
{
    friend class TileEditorPanel;
public:
    MaterialRepository(vio::IOManager& ioManager);
    ~MaterialRepository();

    bool loadMaterial(const vio::Path& filePath, TextureRepository& textureRepository);

    const MaterialGpuData& getMaterial(MaterialID materialId) const;
    MaterialGpuData& getMutableMaterial(MaterialID materialId);
    const MaterialGpuData& getMaterial(const nString& materialName) const;
    MaterialGpuData& getMutableMaterial(const nString& materialName);
    MaterialID getMaterialId(const nString& materialName) const;
    MaterialHandle getMutableMaterialHandle(const nString& materialName);
    MaterialData getMaterialData(const nString& materialName) const;
    MaterialData getMaterialData(MaterialID materialId) const;

    void uploadMaterialData();
    void bindMaterialBuffer() const;

private:

    std::vector<MaterialData> mMaterialData;
    std::vector<MaterialGpuData> mMaterialGpuData;
    std::map<nString, GLTexture> mGeneratedNormalTextures;
    std::map<nString, MaterialID> mMaterialIDLookup;

    vio::IOManager& mIoManager;
    std::unique_ptr<NormalMapGenerator> mNormalMapGenerator;

    GLBuffer mMaterialDataBuffer;
};

