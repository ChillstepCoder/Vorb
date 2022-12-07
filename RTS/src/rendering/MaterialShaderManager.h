#pragma once

#include "rendering/MaterialShader.h"

class TextureRepository;
DECL_VIO(class Path);
DECL_VIO(class IOManager);
DECL_VG(class TextureCache);
DECL_VG(class GLProgram);

class MaterialShaderManager {
public:
    MaterialShaderManager(vio::IOManager& ioManager, TextureRepository& textureRepository, vg::TextureCache& textureCache);
    ~MaterialShaderManager();

    bool loadMaterialShader(const vio::Path& filePath);
    bool loadComputeShader(const vio::Path& filePath);
    const MaterialShader* getMaterialShader(const nString& strId) const;
    const vg::GLProgram* getComputeShader(const nString& strId) const;

private:
    std::vector<std::unique_ptr<MaterialShader>> mMaterialShaders;
    std::unordered_map<nString, ui32> mNameToMaterialShaderMap;
    std::map<nString, vg::GLProgram> mComputeShaders;

    vio::IOManager& mIoManager;
    vg::TextureCache& mTextureCache;
    TextureRepository& mTextureRepository;
};