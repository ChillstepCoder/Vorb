#pragma once

#include "definitions/ModelDef.h"
#include "rendering/mesh/RawMesh.h"

DECL_VIO(class IOManager);
DECL_VG(class TextureCache);
DECL_VG(class Texture);

class MaterialRepository;
class RigRepository;
class AnimMachineRepository;

class ModelRepository
{
    friend class TileEditorPanel; // TODO: ModelEditorPanel only?
public:
    ModelRepository(vio::IOManager& ioManager, const RigRepository& rigRepository);
    ~ModelRepository();

    bool loadModelFile(const vio::Path& filePath, const MaterialRepository& materialRepository, const AnimMachineRepository& animMachineRepository);
    bool loadFbxFile(const vio::Path& filePath, const MaterialRepository& materialRepository, const AnimMachineRepository& animMachineRepository);

    const ModelDef& getModelDef(ModelID modelId) const { return *mModelDefs[modelId]; }
    const ModelDef& getModelDef(const nString& name) const;
    ModelID getModelID(const nString& name) const;

private:
    bool loadSkinnedModel(ModelDefFileData& fileData, const MaterialRepository& materialRepository, const AnimMachineRepository& animMachineRepository, const vio::Path& filePath, const vio::Path& modelPath, const vio::Path& rootDir);
    bool loadStaticModel(ModelDefFileData& fileData, const MaterialRepository& materialRepository, const vio::Path& filePath, const vio::Path& modelPath, const vio::Path& rootDir);
    bool loadRawModel(const vio::Path& filePath);

    const RigRepository& mRigRepository;
    vio::IOManager& mIoManager;
    std::map<nString, ModelID> mModelIdLookup;
    std::vector<std::unique_ptr<ModelDef>> mModelDefs;
    std::map<nString, std::unique_ptr<RawMesh>> mRawModels;
};

