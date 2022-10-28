#pragma once

#include "definitions/ModelDef.h"

DECL_VIO(class IOManager);
DECL_VG(class TextureCache);
DECL_VG(class Texture);

class RigRepository;
class AnimMachineRepository;

class ModelRepository
{
public:
    ModelRepository(vio::IOManager& ioManager, vg::TextureCache& textureCache, const RigRepository& rigRepository);
    ~ModelRepository();

    bool loadModelFile(const vio::Path& filePath, const AnimMachineRepository& animMachineRepository);

    const ModelDef& getModelDef(ModelID modelId) const { return mModelDefs[modelId]; }
    const ModelDef& getModelDef(const nString& name) const;

private:

    const RigRepository& mRigRepository;
    vio::IOManager& mIoManager;
    vg::TextureCache& mTextureCache;
    std::unordered_map<nString, ModelID> mModelIdLookup;
    std::vector<ModelDef> mModelDefs;
};

