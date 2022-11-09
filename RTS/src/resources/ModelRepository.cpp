#include "stdafx.h"
#include "ModelRepository.h"

#include <Vorb/io/IOManager.h>
#include <Vorb/graphics/TextureCache.h>

#include "definitions/AnimMachineDef.h"
#include "resources/RigRepository.h"
#include "resources/AnimMachineRepository.h"
#include "rendering/model/Model3D.h"
#include "rendering/model/ModelVertex.h"
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
bool ModelRepository::loadModelFile(const vio::Path& filePath, const TextureRepository& textureRepository, const AnimMachineRepository& animMachineRepository) {

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
        return loadSkinnedModel(fileData, textureRepository, animMachineRepository, filePath, modelPath, rootDir);
    }
    else {
        return loadStaticModel(fileData, filePath, modelPath, rootDir);
    }
    return false;
}

bool ModelRepository::loadSkinnedModel(ModelDefFileData& fileData, const TextureRepository& textureRepository, const AnimMachineRepository& animMachineRepository, const vio::Path& filePath, vio::Path& modelPath, vio::Path rootDir) {

    ModelDef& def = mModelDefs.emplace_back();
    def.mModelId = (ui32)(mModelDefs.size() - 1u);
    def.mModelType = Model3DType::SKINNED;

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

    // Copy all meshes
    SkinnedModel3D& model = def.getSkinnedModel();
   
    ModelMeshBuilder meshBuilder;
    meshBuilder.buildSkinnedMeshesForModel(model, def.mRig->mSkeleton, filePath, rootDir, sceneLoader, MeshDrawMode::STATIC, textureRepository);

    // Store lookup
    const nString modelFileNameNoExtension = filePath.getFileNameNoExtension();
    assert(mModelIdLookup.find(modelFileNameNoExtension) == mModelIdLookup.end());
    mModelIdLookup[modelFileNameNoExtension] = def.mModelId;
    return true;
}

bool ModelRepository::loadStaticModel(ModelDefFileData& fileData, const vio::Path& filePath, vio::Path& modelPath, vio::Path rootDir) {
    //ModelDef& def = mModelDefs.emplace_back();
    //def.mModelType = Model3DType::STATIC;
    //def.mModelId = (ui32)(mModelDefs.size() - 1u);

    //PreciseTimer timer;
    //nString modelFileNameNoExtension = filePath.getFileNameNoExtension();

    //// Import Fbx content.
    //ozz::animation::offline::fbx::FbxManagerInstance fbxManager;
    //ozz::animation::offline::fbx::FbxDefaultIOSettings settings(fbxManager);
    //ozz::animation::offline::fbx::FbxSceneLoader sceneLoader((const char*)modelPath.getCString(), "", fbxManager, settings);
    //if (!sceneLoader.scene()) {
    //    pError("Failed to import fbx scene: " + filePath.getString());
    //    return false;
    //}
    //LOG_TRACE("  Import in {} ms", timer.stop());
    //timer.start();

    //const int numMeshes = sceneLoader.scene()->GetSrcObjectCount<FbxMesh>();
    //if (numMeshes == 0) {
    //    pError("No mesh to process in this file: " + filePath.getString());
    //    return false;
    //}

    //// Copy all meshes
    //StaticModel3D& model = def.getStaticModel();
    //model.mMeshes = std::unique_ptr<Mesh[]>(new Mesh[numMeshes]);
    //model.mNumMeshes = numMeshes;

    //ModelMeshBuilder meshBuilder;
    //for (int m = 0; m < numMeshes; ++m) {
    //    meshBuilder.buildStaticMesh(
    //        model.mMeshes[m],
    //        m,
    //        filePath,
    //        sceneLoader,
    //        MeshDrawMode::STATIC
    //    );
    //}

    //// Find and load textures
    //// Right now, textures are shared with every mesh in the scene
    //vio::Path textureDir = rootDir + vio::Path("\\") + vio::Path(modelFileNameNoExtension) + vio::Path(".fbm");
    //if (textureDir.isDirectory()) {
    //    vio::Path textureNameRoot = textureDir + vio::Path("\\") + vio::Path(modelFileNameNoExtension) + vio::Path("_");

    //    vio::Path diffusePath = textureNameRoot + vio::Path("diffuse.png");
    //    if (diffusePath.isValid()) {
    //        vg::Texture tex = mTextureCache.addTexture(diffusePath, vg::TextureTarget::TEXTURE_2D, &vg::sSamplerStates.LINEAR_CLAMP_MIPMAP, vg::TextureInternalFormat::RGBA8, vg::TextureFormat::RGBA, INT_MAX, true);
    //        for (int i = 0; i < numMeshes; ++i) {
    //            model.mMeshes[i].setDiffuseTexture(tex.id);
    //        }
    //    }

    //    vio::Path normalPath = textureNameRoot + vio::Path("normal.png");
    //    if (normalPath.isValid()) {
    //        vg::Texture tex = mTextureCache.addTexture(normalPath, vg::TextureTarget::TEXTURE_2D, &vg::sSamplerStates.LINEAR_CLAMP_MIPMAP, vg::TextureInternalFormat::RGBA8, vg::TextureFormat::RGBA, INT_MAX, true);
    //        for (int i = 0; i < numMeshes; ++i) {
    //            model.mSkinnedMeshes[i].setNormalTexture(tex.id);
    //        }
    //    }

    //    vio::Path specularPath = textureNameRoot + vio::Path("specular.png");
    //    if (specularPath.isValid()) {
    //        vg::Texture tex = mTextureCache.addTexture(specularPath, vg::TextureTarget::TEXTURE_2D, &vg::sSamplerStates.LINEAR_CLAMP_MIPMAP, vg::TextureInternalFormat::RGBA8, vg::TextureFormat::RGBA, INT_MAX, true);
    //        for (int i = 0; i < numMeshes; ++i) {
    //            model.mSkinnedMeshes[i].setSpecularTexture(tex.id);
    //        }
    //    }
    //}

    //// Store lookup
    //assert(mModelIdLookup.find(modelFileNameNoExtension) == mModelIdLookup.end());
    //mModelIdLookup[modelFileNameNoExtension] = def.mModelId;
    //return true;
    return false;
}

const ModelDef& ModelRepository::getModelDef(const nString& name) const
{
    auto&& it = mModelIdLookup.find(name);
    assert(it != mModelIdLookup.end());
    return mModelDefs[it->second];
}

ModelID ModelRepository::getModelID(const nString& name) const {
    auto&& it = mModelIdLookup.find(name);
    if (it == mModelIdLookup.end()) {
        LOG_CRITICAL("Model {} not found", name);
        return INVALID_MODEL_ID;
    }
    return it->second;
}
