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

    const MaterialData& getMaterial(MaterialID materialId) const;
    MaterialData& getMutableMaterial(MaterialID materialId);
    const MaterialData& getMaterial(const nString& materialName) const;
    MaterialData& getMutableMaterial(const nString& materialName);
    MaterialID getMaterialId(const nString& materialName) const;
    MaterialHandle getMutableMaterialHandle(const nString& materialName);

    void uploadMaterialData();
    void bindMaterialBuffer() const;

private:

    std::vector<MaterialData> mMaterials;
    std::map<nString, GLTexture> mGeneratedNormalTextures;
    std::map<nString, MaterialID> mMaterialIDLookup;

    vio::IOManager& mIoManager;
    std::unique_ptr<NormalMapGenerator> mNormalMapGenerator;

    GLBuffer mMaterialDataBuffer;
};

