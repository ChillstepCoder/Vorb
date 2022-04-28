#pragma once

#include "rendering/Material.h"

class TextureRepository;
typedef int MaterialID;
DECL_VIO(class Path);
DECL_VIO(class IOManager);
DECL_VG(class TextureCache);

class MaterialManager {
public:
    MaterialManager(vio::IOManager& ioManager, TextureRepository& textureRepository, vg::TextureCache& textureCache);
    ~MaterialManager();

    bool loadMaterial(const vio::Path& filePath);
    const Material* getMaterial(MaterialID id) const;
    const Material* getMaterial(const nString& strId) const;

private:
    std::vector<std::unique_ptr<Material>> mMaterials;
    std::unordered_map<nString, MaterialID> mNameToMaterialIDMap;
    vio::IOManager& mIoManager;
    vg::TextureCache& mTextureCache;
    TextureRepository& mTextureRepository;
};