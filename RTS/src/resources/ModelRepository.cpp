#include "stdafx.h"
#include "ModelRepository.h"

#include <Vorb/io/IOManager.h>
#include <Vorb/graphics/TextureCache.h>

#include "definitions/AnimMachineDef.h"
#include "resources/RigRepository.h"
#include "resources/AnimMachineRepository.h"
#include "rendering/model/Model3D.h"
#include "rendering/mesh/ModelMeshBuilder.h"
#include "rendering/mesh/MeshOperations.h"
#include "rendering/mesh/Mesh.h"

#include <ozz/base/io/archive.h>
#include <ozz/base/io/stream.h>
#include <ozz/animation/runtime/skeleton.h>

#include "rendering/mesh/fbx2raw.inl"

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


#define NEW_METHOD 1

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

#if NEW_METHOD == 1
    RawMesh* rawMesh = loadRawModelFromFBX(modelPath, &def.mRig->mSkeleton);
    if (rawMesh) {
        assert(rawMesh->mCombinedMeshData.mHasSkin);

        // TODO: Handle other submeshes?
        MeshCpuData meshData = ModelMeshBuilder::buildRuntimeOptimizedMeshFromRawMesh(rawMesh->mCombinedMeshData, rawMesh->mMaterials, materialRepository);
        // Apply scale if needed
        if (fileData.mScale != 1.0f) {
            MeshOperations::applyScale(meshData, fileData.mScale);
        }
        SkinnedModel3D& model = def.getSkinnedModel();
        RawMeshSkeletonData& rawSkeletonData = rawMesh->mCombinedMeshData.mSkeletonData;

        model.mNumSkinningMatrices = rawSkeletonData.mNumJoints;
        model.mSkinnedMesh = std::make_unique<Mesh>();

        // Allocate and fill skeleton data
        model.mSkinnedMesh->mSkeletonData = std::make_unique<MeshSkeletonData>();
        MeshSkeletonData& skeletonData = *model.mSkinnedMesh->mSkeletonData;
        skeletonData.mNumJoints = rawSkeletonData.mNumJoints;
        skeletonData.mJointRemaps = std::unique_ptr<ui8[]>(new ui8[skeletonData.mNumJoints]);
        memcpy(skeletonData.mJointRemaps.get(), rawSkeletonData.mJointRemaps.data(), sizeof(ui8) * skeletonData.mNumJoints);
        skeletonData.mInverseBindPoses = std::unique_ptr<ozz::math::Float4x4[]>(new ozz::math::Float4x4[skeletonData.mNumJoints]);
        memcpy(skeletonData.mInverseBindPoses.get(), rawSkeletonData.mInverseBindPoses.data(), sizeof(ozz::math::Float4x4) * skeletonData.mNumJoints);

        ModelMeshBuilder::uploadCpuMeshToGpu(meshData, model.mSkinnedMesh->mMainMesh);

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
    return false;
#else

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
    ModelMeshBuilder::buildSkinnedMeshesForModel(model, def.mRig->mSkeleton, filePath, sceneLoader, MeshDrawMode::STATIC, materialRepository);

    // Store lookup
    const nString modelFileNameNoExtension = filePath.getFileNameNoExtension();
    assert(mModelIdLookup.find(modelFileNameNoExtension) == mModelIdLookup.end());
    mModelIdLookup[modelFileNameNoExtension] = def.mModelId;
    // TODO: Don't use extra lookup to copy the name?
    def.mName = mModelIdLookup.find(modelFileNameNoExtension)->first.c_str();
    return true;
#endif
}

bool ModelRepository::loadStaticModel(ModelDefFileData& fileData, const MaterialRepository& materialRepository, const vio::Path& filePath, const vio::Path& modelPath, const vio::Path& rootDir) {

    PROFILE_FUNCTION();

    ModelDef& def = *mModelDefs.emplace_back(std::make_unique<ModelDef>());
    def.mModelType = Model3DType::STATIC;
    def.mModelId = (ui32)(mModelDefs.size() - 1u);
    def.mShadowDetail = fileData.mShadowDetail;

    PreciseTimer timer;

#if NEW_METHOD == 1
    RawMesh* rawMesh = loadRawModelFromFBX(modelPath, nullptr /*skeleton*/);
    if (rawMesh) {
        // TODO: Handle other submeshes?
        MeshCpuData meshData = ModelMeshBuilder::buildRuntimeOptimizedMeshFromRawMesh(rawMesh->mCombinedMeshData, rawMesh->mMaterials, materialRepository);
        // Apply scale if needed
        if (fileData.mScale != 1.0f) {
            MeshOperations::applyScale(meshData, fileData.mScale);
        }
        StaticModel3D& model = def.getStaticModel();

        model.mMesh = std::make_unique<Mesh>();
        ModelMeshBuilder::uploadCpuMeshToGpu(meshData, model.mMesh->mMainMesh);

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
    return false;
#else
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
    ModelMeshBuilder::buildStaticMeshesForModel(model, filePath, sceneLoader, MeshDrawMode::STATIC, materialRepository, fileData.mScale);

    // Store lookup
    const nString modelFileNameNoExtension = filePath.getFileNameNoExtension();
    if (mModelIdLookup.find(modelFileNameNoExtension) != mModelIdLookup.end()) {
        LOG_INFO("Replacing model {}", filePath.getCString());
    }
    mModelIdLookup[modelFileNameNoExtension] = def.mModelId;
    // TODO: Don't use extra lookup to copy the name?
    def.mName = mModelIdLookup.find(modelFileNameNoExtension)->first.c_str();
    return true;
#endif
}

RawMesh* ModelRepository::loadRawModelFromFBX(const vio::Path& filePath, const ozz::animation::Skeleton* skeleton) {

    // Load scene
    ozz::animation::offline::fbx::FbxManagerInstance fbxManager;
    ozz::animation::offline::fbx::FbxDefaultIOSettings settings(fbxManager);
    ozz::animation::offline::fbx::FbxSceneLoader sceneLoader((const char*)filePath.getCString(), "", fbxManager, settings);
    if (!sceneLoader.scene()) {
        pError("Failed to import fbx scene: " + filePath.getString());
        return nullptr;
    }

    const int numMeshes = sceneLoader.scene()->GetSrcObjectCount<FbxMesh>();
    if (numMeshes == 0) {
        pError("No mesh to process in this file: " + filePath.getString());
        return nullptr;
    }

    std::unique_ptr<RawMesh> rawFbxMesh = std::make_unique<RawMesh>();
    RawMesh* rv = rawFbxMesh.get();

    const nString modelName = filePath.getFileNameNoExtension();

    // Materials
    const int materialCount = sceneLoader.scene()->GetMaterialCount();
    rawFbxMesh->mMaterials.resize(materialCount);
    for (int i = 0; i < materialCount; ++i) {
        FbxSurfaceMaterial* fbxMaterial = sceneLoader.scene()->GetMaterial(i);
        assert(fbxMaterial);
        rawFbxMesh->mMaterials[i] = fbx2raw::readFbxMaterial(*fbxMaterial);
    }

    // Meshes
    ui32 totalVertices = 0;
    ui32 totalIndices = 0;
    int hasSkin = INT32_MAX;
    rawFbxMesh->mSubMeshes.reserve(numMeshes);
    for (int m = 0; m < numMeshes; ++m) {

        FbxMesh* fbxMesh = sceneLoader.scene()->GetSrcObject<FbxMesh>(m);
        RawSubMesh& subMesh = rawFbxMesh->mSubMeshes.emplace_back();

        ControlPointsRemap remap;
        if (!fbx2raw::buildRawSubmesh(fbxMesh, sceneLoader.converter(), &remap, subMesh, rawFbxMesh->mMaterials)) {
            LOG_CRITICAL("Failed to build submesh {} for {}", m, filePath.getString());
            pError("Failed to read submesh for: " + filePath.getString());
            return nullptr;
        }

        if (fbxMesh->GetDeformerCount(FbxDeformer::eSkin) > 0) {
            assert(hasSkin == true || hasSkin == INT32_MAX);
            subMesh.mHasSkin = true;
            hasSkin = true;
            assert(skeleton && "Needs to have skeleton explicitly passed in");
            if (!fbx2raw::buildSkin(fbxMesh, sceneLoader.converter(), remap, *skeleton, subMesh)) {
                LOG_CRITICAL("Failed to read skinning data {} for {}", m, filePath.getString());
                pError("Failed to read skinning data: " + filePath.getString());
                return nullptr;
            }
        }
        else {
            assert(hasSkin == false || hasSkin == INT32_MAX);
            subMesh.mHasSkin = false;
            hasSkin = false;
        }

        totalVertices += subMesh.mVertices.size();
        totalIndices += subMesh.mIndices.size();
    }
    assert(hasSkin != INT32_MAX);

    // Combine all submeshes
    rv->mCombinedMeshData.mVertices.resize(totalVertices);
    rv->mCombinedMeshData.mIndices.resize(totalIndices);
    rv->mCombinedMeshData.mHasSkin = hasSkin;
    if (hasSkin) {
        assert(numMeshes == 1 && "Currently skinned meshes must be a single submesh only");
        rv->mCombinedMeshData.mSkeletonData = std::move(rawFbxMesh->mSubMeshes[0].mSkeletonData);
    }
    int v = 0;
    int i = 0;
    int iStart = 0;
    for (int m = 0; m < numMeshes; ++m) {
        const RawSubMesh& subMesh = rawFbxMesh->mSubMeshes[m];
        for (int j = 0; j < subMesh.mVertices.size(); ++j) {
            rv->mCombinedMeshData.mVertices[v++] = subMesh.mVertices[j];
        }
        for (int j = 0; j < subMesh.mIndices.size(); ++j) {
            rv->mCombinedMeshData.mIndices[i++] = subMesh.mIndices[j] + iStart;
        }
        iStart += subMesh.mVertices.size();
    }

    mRawModels[std::move(modelName)] = std::move(rawFbxMesh);
    return rv;
}

const ModelDef& ModelRepository::getModelDef(const nString& name) const {
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

void ModelRepository::buildModelBatches() {
    assert(false);
}
