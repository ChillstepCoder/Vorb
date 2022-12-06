#pragma once

#include "rendering/MaterialShader.h"

class TextureRepository;
typedef int MaterialID;
DECL_VIO(class Path);
DECL_VIO(class IOManager);
DECL_VG(class TextureCache);
DECL_VG(class GLProgram);

class MaterialManager {
public:
    MaterialManager(vio::IOManager& ioManager, TextureRepository& textureRepository, vg::TextureCache& textureCache);
    ~MaterialManager();

    bool loadMaterialShader(const vio::Path& filePath);
    bool loadComputeShader(const vio::Path& filePath);
    const MaterialShader* getMaterial(MaterialID id) const;
    const MaterialShader* getMaterial(const nString& strId) const;
    const vg::GLProgram* getComputeShader(const nString& strId) const;

private:
    std::vector<std::unique_ptr<MaterialShader>> mMaterials;
    std::unordered_map<nString, MaterialID> mNameToMaterialIDMap;
    std::map<nString, vg::GLProgram> mComputeShaders;
    vio::IOManager& mIoManager;
    vg::TextureCache& mTextureCache;
    TextureRepository& mTextureRepository;
};