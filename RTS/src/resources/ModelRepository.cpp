#include "stdafx.h"
#include "ModelRepository.h"

#include <Vorb/io/IOManager.h>

#include "definitions/AnimMachineDef.h"
#include "resources/RigRepository.h"
#include "resources/AnimMachineRepository.h"
#include "rendering/mesh/mesher/builder/ModelMeshBuilder.h"
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

    const vio::Path modelPath = rootDir + nString("\\") + fileData.mModelName;
    return loadModelInternal(fileData, materialRepository, animMachineRepository, filePath.getFileNameNoExtension(), modelPath);
}

bool ModelRepository::loadFbxFile(const vio::Path& filePath, const MaterialRepository& materialRepository, const AnimMachineRepository& animMachineRepository) {
    PROFILE_FUNCTION();

    LOG_TRACE("Loading FBX {}", filePath.getCString());

    vio::Path rootDir = filePath;
    rootDir.trimEnd();
    assert(rootDir.isDirectory());

    ModelDefFileData fileData;
    return loadModelInternal(fileData, materialRepository, animMachineRepository, filePath.getFileNameNoExtension(), filePath);
}

bool ModelRepository::loadModelInternal(ModelDefFileData& fileData, const MaterialRepository& materialRepository, const AnimMachineRepository& animMachineRepository, const nString& modelName, const vio::Path& modelPath) {
    // Create the modeldef
    ModelDef& def = *mModelDefs.emplace_back(std::make_unique<ModelDef>());
    def.mModelId = (ui32)(mModelDefs.size() - 1u);
    def.mShadowDetail = fileData.mShadowDetail;

    // If has rig, we need to load animation and skeleton info
    if (fileData.mRigName.size()) {
        def.mRig = &mRigRepository.getRigDef(fileData.mRigName);

        if (fileData.mMachineName.size()) {
            const AnimMachineDef* animMachineDef = animMachineRepository.tryGetAnimMachineDef(fileData.mMachineName);
            if (!animMachineDef) {
                pError("Failed to find anim machine " + fileData.mMachineName + " for: " + modelPath.getString());
                return false;
            }
            def.mAnimMachine = animMachineDef;
        }
    }

    // Load model to raw
    RawMesh* rawMesh = loadRawModelFromFBX(modelPath, &def.mRig->mSkeleton);
    if (rawMesh) {
        if (fileData.mForceNormalsUp) {
            MeshOperations::setAllNormals(*rawMesh, f32v3(0.0f, 0.0f, 1.0f), f32v3(1.0f, 0.0f, 0.0f));
        }


        // TODO: Handle other submeshes?
        MeshCpuData meshData = ModelMeshBuilder::buildRuntimeOptimizedMeshFromRawMesh(rawMesh->mCombinedMeshData, rawMesh->mMaterials, materialRepository);
        // Apply scale if needed
        if (fileData.mScale != 1.0f) {
            MeshOperations::applyScale(meshData, fileData.mScale);
        }
        RawMeshSkeletonData& rawSkeletonData = rawMesh->mCombinedMeshData.mSkeletonData;

        def.mMesh = std::make_unique<Mesh>();

        // Allocate and fill skeleton data
        if (rawSkeletonData.mNumJoints) {
            assert(def.mRig && "Missing rig for skeletal model");
            def.mMesh->mSkeletonData = std::make_unique<MeshSkeletonData>();
            MeshSkeletonData& skeletonData = *def.mMesh->mSkeletonData;
            skeletonData.mNumJoints = rawSkeletonData.mNumJoints;
            skeletonData.mJointRemaps = std::unique_ptr<ui8[]>(new ui8[skeletonData.mNumJoints]);
            memcpy(skeletonData.mJointRemaps.get(), rawSkeletonData.mJointRemaps.data(), sizeof(ui8) * skeletonData.mNumJoints);
            skeletonData.mInverseBindPoses = std::unique_ptr<ozz::math::Float4x4[]>(new ozz::math::Float4x4[skeletonData.mNumJoints]);
            memcpy(skeletonData.mInverseBindPoses.get(), rawSkeletonData.mInverseBindPoses.data(), sizeof(ozz::math::Float4x4) * skeletonData.mNumJoints);
        }

        ModelMeshBuilder::uploadCpuMeshToGpu(meshData, def.mMesh->mMainMesh);

        // Store lookup
        if (mModelIdLookup.find(modelName) != mModelIdLookup.end()) {
            LOG_INFO("Replacing model {}", modelName);
        }
        mModelIdLookup[modelName] = def.mModelId;
        // TODO: Don't use extra lookup to copy the name?
        def.mName = mModelIdLookup.find(modelName)->first.c_str();
        return true;
    }

    // Failure
    mModelDefs.pop_back();
    return false;
}

RawMesh* ModelRepository::loadRawModelFromFBX(const vio::Path& filePath, const ozz::animation::Skeleton* skeleton) {

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
