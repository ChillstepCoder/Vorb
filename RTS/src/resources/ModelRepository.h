#pragma once

#include "definitions/ModelDef.h"

DECL_VIO(class IOManager);
DECL_VG(class TextureCache);
DECL_VG(class Texture);

typedef ui32 ModelID;

struct aiScene;
struct aiMaterial;
struct aiString;
enum aiTextureType;

class ModelRepository
{
public:
    ModelRepository(vio::IOManager& ioManager, vg::TextureCache& textureCache);
    ~ModelRepository();

    bool loadModelFile(const vio::Path& filePath);

    const ModelDef& getModelDef(ui32 modelId) const { return mModelDefs[modelId]; }
    const ModelDef& getModelDef(const nString& name);

private:
    vg::Texture createGlTextureFromAiTexture(aiMaterial* material, const aiTextureType& textureType, aiString& texPath, const aiScene* aiScene);

    vio::IOManager& mIoManager;
    vg::TextureCache& mTextureCache;
    std::unordered_map<nString, ui32> mModelIdLookup;
    std::vector<ModelDef> mModelDefs;
};

