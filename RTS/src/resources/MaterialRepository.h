#pragma once

#include "rendering/material/MaterialData.h"
#include "rendering/texture/GLTexture.h"

DECL_VIO(class IOManager);

class TextureRepository;
class NormalMapGenerator;

class MaterialRepository
{
public:
    MaterialRepository(vio::IOManager& ioManager);
    ~MaterialRepository();

    bool loadMaterial(const vio::Path& filePath, TextureRepository& textureRepository);
    const MaterialData& getMaterial(MaterialID materialId) const;
    const MaterialData& getMaterial(const nString& materialName) const;
    MaterialID getMaterialId(const nString& materialName) const;
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

