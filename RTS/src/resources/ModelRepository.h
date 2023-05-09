#pragma once

#include "definitions/ModelDef.h"
#include "rendering/mesh/RawMesh.h"

DECL_VIO(class IOManager);
DECL_VG(class TextureCache);
DECL_VG(class Texture);

class MaterialRepository;
class RigRepository;
class AnimMachineRepository;

namespace ozz::animation {
    class Skeleton;
};

// TODO: MOVE
constexpr GLuint MODEL_TRANSFORMS_BINDING_POINT = 2;

class ModelRepository
{
    friend class TileEditorPanel;
public:
    ModelRepository(vio::IOManager& ioManager, const RigRepository& rigRepository);
    ~ModelRepository();

    bool loadModelFile(const vio::Path& filePath, const MaterialRepository& materialRepository, const AnimMachineRepository& animMachineRepository);
    bool loadFbxFile(const vio::Path& filePath, const MaterialRepository& materialRepository, const AnimMachineRepository& animMachineRepository);

    const ModelDef& getModelDef(ModelID modelId) const { return *mModelDefs[modelId]; }
    const ModelDef& getModelDef(const nString& name) const;
    ModelID getModelID(const nString& name) const;

    void buildModelBatches();

private:
    bool loadModelInternal(ModelDefFileData& fileData, const MaterialRepository& materialRepository, const AnimMachineRepository& animMachineRepository, const nString& modelName, const vio::Path& modelPath);
    RawMesh* loadRawModelFromFBX(const vio::Path& filePath, const ozz::animation::Skeleton* skeleton);

    const RigRepository& mRigRepository;
    vio::IOManager& mIoManager;
    std::map<nString, ModelID> mModelIdLookup;
    std::vector<std::unique_ptr<ModelDef>> mModelDefs;
    std::map<nString, std::unique_ptr<RawMesh>> mRawModels;

    // TODO: Pooled allocate
    std::vector<std::unique_ptr<ModelBatch>> mModelBatches;
};

