#include "stdafx.h"
#include "ModelRepository.h"

#include <Vorb/io/IOManager.h>
#include <Vorb/graphics/TextureCache.h>

#include "definitions/AnimMachineDef.h"
#include "resources/RigRepository.h"
#include "resources/AnimMachineRepository.h"
#include "rendering/model/Model3D.h"
#include "rendering/mesh/ModelMeshBuilder.h"

#include <ozz/base/io/archive.h>
#include <ozz/base/io/stream.h>
#include <ozz/animation/runtime/skeleton.h>
#include <ozz/animation/offline/fbx/fbx.h>

ModelRepository::ModelRepository(vio::IOManager& ioManager, const RigRepository& rigRepository) : mIoManager(ioManager), mRigRepository(rigRepository) {

}

ModelRepository::~ModelRepository() {

}

// TODO: Cache model files in binary
bool ModelRepository::loadModelFile(const vio::Path& filePath, const MaterialRepository& materialRepository, const AnimMachineRepository& animMachineRepository) {

    PROFILE_FUNCTION();

    LOG_TRACE("Loading model {}", filePath.getCString());

    ModelDefFileData fileData;
    if (!mIoManager.parseFileAsKegObject((ui8*)&fileData, filePath, &KEG_GLOBAL_TYPE(ModelDefFileData))) {
        pError("Failed to load model file " + filePath.getString());
        return false;
    }

    if (fileData.mModelName.empty()) {
        pError("Model file missing model name " + filePath.getString());
        return false;
    }

    vio::Path rootDir = filePath;
    rootDir.trimEnd();
    assert(rootDir.isDirectory());

    vio::Path modelPath = rootDir + nString("\\") + fileData.mModelName;

    if (fileData.mRigName.size()) {
        // Load ozz Skeleton if we use it
        return loadSkinnedModel(fileData, materialRepository, animMachineRepository, filePath, modelPath, rootDir);
    }
    else {
        return loadStaticModel(fileData, materialRepository, filePath, modelPath, rootDir);
    }
    return false;
}

bool ModelRepository::loadFbxFile(const vio::Path& filePath, const MaterialRepository& materialRepository, const AnimMachineRepository& animMachineRepository) {
    PROFILE_FUNCTION();

    LOG_TRACE("Loading FBX {}", filePath.getCString());

    vio::Path rootDir = filePath;
    rootDir.trimEnd();
    assert(rootDir.isDirectory());

    ModelDefFileData fileData;
    return loadStaticModel(fileData, materialRepository, filePath, filePath, rootDir);
}

bool ModelRepository::loadSkinnedModel(ModelDefFileData& fileData, const MaterialRepository& materialRepository, const AnimMachineRepository& animMachineRepository, const vio::Path& filePath, const vio::Path& modelPath, const vio::Path& rootDir) {

    PROFILE_FUNCTION();

    ModelDef& def = *mModelDefs.emplace_back(std::make_unique<ModelDef>());
    def.mModelId = (ui32)(mModelDefs.size() - 1u);
    def.mModelType = Model3DType::SKINNED;
    def.mShadowDetail = fileData.mShadowDetail;

    PreciseTimer timer;
    def.mRig = &mRigRepository.getRigDef(fileData.mRigName);

    // Hookup animation machine
    if (fileData.mMachineName.size()) {
        const AnimMachineDef* animMachineDef = animMachineRepository.tryGetAnimMachineDef(fileData.mMachineName);
        if (!animMachineDef) {
            pError("Failed to find anim machine " + fileData.mMachineName + " for: " + filePath.getString());
            return false;
        }
        def.mAnimMachine = animMachineDef;
    }

    // Import Fbx content.
    ozz::animation::offline::fbx::FbxManagerInstance fbxManager;
    ozz::animation::offline::fbx::FbxDefaultIOSettings settings(fbxManager);
    ozz::animation::offline::fbx::FbxSceneLoader sceneLoader((const char*)modelPath.getCString(), "", fbxManager, settings);
    if (!sceneLoader.scene()) {
        pError("Failed to import fbx scene: " + filePath.getString());
        return false;
    }
    LOG_TRACE("  Import in {} ms", timer.stop());
    timer.start();

    SkinnedModel3D& model = def.getSkinnedModel();
    ModelMeshBuilder meshBuilder;
    meshBuilder.buildSkinnedMeshesForModel(model, def.mRig->mSkeleton, filePath, rootDir, sceneLoader, MeshDrawMode::STATIC, materialRepository);

    // Store lookup
    const nString modelFileNameNoExtension = filePath.getFileNameNoExtension();
    assert(mModelIdLookup.find(modelFileNameNoExtension) == mModelIdLookup.end());
    mModelIdLookup[modelFileNameNoExtension] = def.mModelId;
    // TODO: Don't use extra lookup to copy the name?
    def.mName = mModelIdLookup.find(modelFileNameNoExtension)->first.c_str();
    return true;
}

bool ModelRepository::loadStaticModel(ModelDefFileData& fileData, const MaterialRepository& materialRepository, const vio::Path& filePath, const vio::Path& modelPath, const vio::Path& rootDir) {

    PROFILE_FUNCTION();

    ModelDef& def = *mModelDefs.emplace_back(std::make_unique<ModelDef>());
    def.mModelType = Model3DType::STATIC;
    def.mModelId = (ui32)(mModelDefs.size() - 1u);
    def.mShadowDetail = fileData.mShadowDetail;

    PreciseTimer timer;

    // Import Fbx content.
    ozz::animation::offline::fbx::FbxManagerInstance fbxManager;
    ozz::animation::offline::fbx::FbxDefaultIOSettings settings(fbxManager);
    ozz::animation::offline::fbx::FbxSceneLoader sceneLoader((const char*)modelPath.getCString(), "", fbxManager, settings);
    if (!sceneLoader.scene()) {
        pError("Failed to import fbx scene: " + filePath.getString());
        return false;
    }
    LOG_TRACE("  Import in {} ms", timer.stop());
    timer.start();

    StaticModel3D& model = def.getStaticModel();
    ModelMeshBuilder meshBuilder;
    meshBuilder.buildStaticMeshesForModel(model, filePath, rootDir, sceneLoader, MeshDrawMode::STATIC, materialRepository, fileData.mScale);

    // Store lookup
    const nString modelFileNameNoExtension = filePath.getFileNameNoExtension();
    if (mModelIdLookup.find(modelFileNameNoExtension) != mModelIdLookup.end()) {
        LOG_INFO("Replacing model {}", filePath.getCString());
    }
    mModelIdLookup[modelFileNameNoExtension] = def.mModelId;
    // TODO: Don't use extra lookup to copy the name?
    def.mName = mModelIdLookup.find(modelFileNameNoExtension)->first.c_str();
    return true;
}

const ModelDef& ModelRepository::getModelDef(const nString& name) const
{
    auto&& it = mModelIdLookup.find(name);
    assert(it != mModelIdLookup.end());
    return *mModelDefs[it->second];
}

ModelID ModelRepository::getModelID(const nString& name) const {
    auto&& it = mModelIdLookup.find(name);
    if (it == mModelIdLookup.end()) {
        LOG_CRITICAL("Model {} not found", name);
        return INVALID_MODEL_ID;
    }
    return it->second;
}
