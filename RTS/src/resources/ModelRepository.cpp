#include "stdafx.h"
#include "ModelRepository.h"

#include <Vorb/io/IOManager.h>

#include "definitions/AnimMachineDef.h"
#include "resources/RigRepository.h"
#include "resources/AnimMachineRepository.h"
#include "rendering/mesh/mesher/builder/ModelMeshBuilder.h"
#include "rendering/mesh/MeshOperations.h"
#include "rendering/mesh/Mesh.h"

#include "resources/MaterialRepository.h"

#include <ozz/base/io/archive.h>
#include <ozz/base/io/stream.h>
#include <ozz/animation/runtime/skeleton.h>

#include "rendering/mesh/fbx2raw.inl"



class FBXLoadContext {
public:
    FBXLoadContext(const char* filePath) : fbxManager(), settings(fbxManager), sceneLoader(filePath, "", fbxManager, settings) {}
    ~FBXLoadContext() {};

    ozz::animation::offline::fbx::FbxManagerInstance fbxManager;
    ozz::animation::offline::fbx::FbxDefaultIOSettings settings;
    ozz::animation::offline::fbx::FbxSceneLoader sceneLoader;
    MeshCpuData meshData[e_count(MaterialRenderPassType)];
};

bool ModelRepository::loadFbxFile(const vio::Path& filePath) {
    PROFILE_FUNCTION();

    LOG_TRACE("Loading FBX {}", filePath.getCString());

    vio::Path rootDir = filePath;
    rootDir.trimEnd();
    assert(rootDir.isDirectory());

    StrToken newName(filePath.getFileNameNoExtension());
    AssetID newId = registerAsset(newName, filePath);

    ModelDef& def = *mAssets[newId];

    panic("Need to finish ModelRepository::loadFbxFile");
    loadModelInternal(def, newName, filePath);
}

void ModelRepository::onAssetChangedByEditor(AssetID id) {
    updateModelFlyweightData(id);
}

AssetLoadFunc ModelRepository::getAssetLoadFunc() {
    return [&]ASSET_LOAD_LAMBDA(assetID, filePath, assetDataPtr) {

        ModelDef& def = *static_cast<ModelDef*>(assetDataPtr);
        //TextureLoadUserData& loadUserData = std::any_cast<TextureLoadUserData&>(userData);

        LOG_TRACE("Loading model {}", filePath.getCString());


        if (!def.mModelName.isValid()) {
            panic("Model file missing model name - {}", filePath.getString());
        }

        vio::Path rootDir = filePath;
        rootDir.trimEnd();
        assert(rootDir.isDirectory());

        const vio::Path modelPath = rootDir + nString("\\") + def.mModelName.toString();
        loadModelInternal(def, def.getName(), modelPath);

        return false;
    };
}

void ModelRepository::loadModelInternal(ModelDef& def, StrToken modelName, const vio::Path& modelPath) {

    MaterialRepository& materialRepo = MaterialRepository::get();
    // Allocate raw FBX
    FBXRawMesh* rawMeshPtr;

    {
        std::unique_ptr<FBXRawMesh> rawFbxMesh = std::make_unique<FBXRawMesh>();
        rawMeshPtr = rawFbxMesh.get();
        std::lock_guard lock(mRawModelsMutex);
        mRawModels[std::move(modelName)] = std::move(rawFbxMesh);
    }

    // If has rig, we need to load animation and skeleton info
    if (def.mRigName.isValid()) {
        def.addDependency(RigRepository::get().getAssetHandle(def.mRigName.name));
        if (def.mMachineName.isValid()) {
            def.addDependency(AnimMachineRepository::get().getAssetHandle(def.mMachineName.name));
        }
    }

    // Material dependencies
    mFbxSdkMutex.lock();
    std::shared_ptr<FBXLoadContext> loadContextPtr = std::make_shared<FBXLoadContext>(modelPath.getCString());
    mFbxSdkMutex.unlock();
    const int materialCount = loadContextPtr->sceneLoader.scene()->GetMaterialCount();
    rawMeshPtr->mMaterials.resize(materialCount);
    for (int i = 0; i < materialCount; ++i) {
        FbxSurfaceMaterial* fbxMaterial = loadContextPtr->sceneLoader.scene()->GetMaterial(i);
        assert(fbxMaterial);
        rawMeshPtr->mMaterials[i] = fbx2raw::readFbxMaterial(*fbxMaterial);
        def.addDependency(materialRepo.getAssetHandle(StrToken(rawMeshPtr->mMaterials[i].materialName)));
    }

    AssetLoader::getInstance().requestAssetLoadWithDependencies([this, rawMeshPtr, materialCount]ASSET_LOAD_LAMBDA(assetId, filePath, assetDataPtr, userData) {
        FBXLoadContext& loadContext = *std::any_cast<std::shared_ptr<FBXLoadContext>&>(userData);

        ModelDef& def = *static_cast<ModelDef*>(assetDataPtr);

        // Rig + animation
        if (def.mRigName.isValid()) {
            def.mRig = &def.getDependencies()->getLoadedAsset<RigDef>(def.mRigName.name);
            if (def.mMachineName.isValid()) {
                def.mAnimMachine = &def.getDependencies()->getLoadedAsset<AnimMachineDef>(def.mMachineName.name);
            }
        }

        // Materials
        for (int i = 0; i < materialCount; ++i) {
            rawMeshPtr->mMaterials[i].materialDef = &def.getDependencies()->getLoadedAsset<MaterialDef>(StrToken(rawMeshPtr->mMaterials[i].materialName));
        }

        // Load model to raw
        loadRawModelFromFBX(loadContext, *rawMeshPtr, filePath, def.mRig ? &def.mRig->mSkeleton : nullptr);
        if (def.mForceNormalsUp) {
            MeshOperations::setAllNormals(*rawMeshPtr, f32v3(0.0f, 0.0f, 1.0f), f32v3(1.0f, 0.0f, 0.0f));
        }

        f32 minX = FLT_MAX;
        f32 maxX = -FLT_MAX;
        f32 minY = FLT_MAX;
        f32 maxY = -FLT_MAX;
        f32 minZ = FLT_MAX;
        f32 maxZ = -FLT_MAX;

        // TODO: Handle other submeshes?
        for (int renderPassType = 0; renderPassType < e_count(MaterialRenderPassType); ++renderPassType) {
            RawSubMesh& combinedMeshData = rawMeshPtr->mCombinedMeshData[renderPassType];
            if (combinedMeshData.mVertices.empty()) {
                continue;
            }
            for (auto& vert : combinedMeshData.mVertices) {
                if (vert.pos.x < minX) minX = vert.pos.x;
                if (vert.pos.x > maxX) maxX = vert.pos.x;
                if (vert.pos.y < minY) minY = vert.pos.y;
                if (vert.pos.y > maxY) maxY = vert.pos.y;
                if (vert.pos.z < minZ) minZ = vert.pos.z;
                if (vert.pos.z > maxZ) maxZ = vert.pos.z;
            }

            loadContext.meshData[def.mNumMeshes] = ModelMeshBuilder::buildRuntimeOptimizedMeshFromRawMesh(combinedMeshData, rawMeshPtr->mMaterials);
            // Apply scale if needed
            if (def.mScale != 1.0f) {
                MeshOperations::applyScale(loadContext.meshData[def.mNumMeshes], def.mScale);
            }
            RawMeshSkeletonData& rawSkeletonData = combinedMeshData.mSkeletonData;

            // Allocate and fill skeleton data
            if (rawSkeletonData.mNumJoints) {
                std::unique_ptr<SkeletalMesh> newMesh = std::make_unique<SkeletalMesh>();
                assert(def.mRig && "Missing rig for skeletal model");
                MeshSkeletonData& skeletonData = newMesh->mSkeletonData;
                skeletonData.mNumJoints = rawSkeletonData.mNumJoints;
                skeletonData.mJointRemaps = std::unique_ptr<ui8[]>(new ui8[skeletonData.mNumJoints]);
                memcpy(skeletonData.mJointRemaps.get(), rawSkeletonData.mJointRemaps.data(), sizeof(ui8) * skeletonData.mNumJoints);
                skeletonData.mInverseBindPoses = std::unique_ptr<ozz::math::Float4x4[]>(new ozz::math::Float4x4[skeletonData.mNumJoints]);
                memcpy(skeletonData.mInverseBindPoses.get(), rawSkeletonData.mInverseBindPoses.data(), sizeof(ozz::math::Float4x4) * skeletonData.mNumJoints);
                def.addMesh(std::move(newMesh));
            }
            else {
                def.addMesh(std::make_unique<Mesh>());
            }

            Mesh& newMesh = *def.mMeshes[def.mNumMeshes - 1];
            newMesh.setRenderPass((MaterialRenderPassType)renderPassType);
        }

        minX *= def.mScale;
        maxX *= def.mScale;
        minY *= def.mScale;
        maxY *= def.mScale;
        minZ *= def.mScale;
        maxZ *= def.mScale;
        def.mAABB = f32AABB3(f32v3(minX, minY, minZ), f32v3(maxX - minX, maxY - minY, maxZ - minZ));

        // Make sure we have proper submesh data linked
        if (def.mNumMeshes != def.mSubmeshesData.size()) {
            def.mSubmeshesData.resize(def.mNumMeshes);
        }
        for (ui32 i = 0; i < def.mNumMeshes; ++i) {
            def.mMeshes[i]->setSubmeshData(&def.mSubmeshesData[i]);
        }

        return true;

    }, [this]ASSET_LOAD_LAMBDA(assetId, filePath, assetDataPtr, userData) {
        ModelDef& def = *static_cast<ModelDef*>(assetDataPtr);
        std::shared_ptr<FBXLoadContext>& loadContextPtr = std::any_cast<std::shared_ptr<FBXLoadContext>&>(userData);
        FBXLoadContext& loadContext = *loadContextPtr;
        for (ui32 i = 0; i < def.mNumMeshes; ++i) {
            if (loadContext.meshData[i].mVertsCount) {
                ModelMeshBuilder::uploadCpuMeshToGpu(loadContext.meshData[i], def.mMeshes[i]->mGpuData);
            }
        }
        mFbxSdkMutex.lock();
        loadContextPtr.reset();
        mFbxSdkMutex.unlock();
        return true;
    },
        def.getID(),
        &def,
        modelPath,
        mLoadedAssets[def.getID()].get(),
        std::move(loadContextPtr),
        def.getDependencies()
    );

}

void ModelRepository::loadRawModelFromFBX(FBXLoadContext& loadContext, FBXRawMesh& rawFbxMesh, const vio::Path& filePath, const ozz::animation::Skeleton* skeleton) {
    MaterialRepository& materialRepo = MaterialRepository::get();
    
    // FBX sdk is not thread safe...
    std::unique_lock lock(mFbxSdkMutex);

    if (!loadContext.sceneLoader.scene()) {
        panic("Failed to import fbx scene: {}", filePath.getString());
    }

    const int numMeshes = loadContext.sceneLoader.scene()->GetSrcObjectCount<FbxMesh>();
    if (numMeshes == 0) {
        panic("No mesh to process in this file: {}", filePath.getString());
    }

    // Meshes
    ui32 totalVertices[e_count(MaterialRenderPassType)] = {};
    ui32 totalIndices[e_count(MaterialRenderPassType)] = {};
    int hasSkin = INT32_MAX;
    rawFbxMesh.mSubMeshes.reserve(numMeshes);
    for (int m = 0; m < numMeshes; ++m) {

        FbxMesh* fbxMesh = loadContext.sceneLoader.scene()->GetSrcObject<FbxMesh>(m);
        RawSubMesh& subMesh = rawFbxMesh.mSubMeshes.emplace_back();

        ControlPointsRemap remap;
        if (!fbx2raw::buildRawSubmesh(fbxMesh, loadContext.sceneLoader.converter(), &remap, subMesh, rawFbxMesh.mMaterials, skeleton == nullptr)) {
            panic("Failed to read submesh for: {}", filePath.getString());
        }

        if (fbxMesh->GetDeformerCount(FbxDeformer::eSkin) > 0) {
            assert(hasSkin == true || hasSkin == INT32_MAX);
            subMesh.mHasSkin = true;
            hasSkin = true;
            assert(skeleton && "Needs to have skeleton explicitly passed in");
            if (!fbx2raw::buildSkin(fbxMesh, loadContext.sceneLoader.converter(), remap, *skeleton, subMesh)) {
                panic("Failed to read skinning data: {}", filePath.getString());
            }
        }
        else {
            assert(hasSkin == false || hasSkin == INT32_MAX);
            subMesh.mHasSkin = false;
            hasSkin = false;
        }

        if (subMesh.mVertices.size()) {
            const int materialIndex = subMesh.mVertices[0].materialIndex;
            assert(rawFbxMesh.mMaterials[materialIndex].materialDef);
            const MaterialRenderPassType renderPass = rawFbxMesh.mMaterials[materialIndex].materialDef->renderPass;
            totalVertices[e_cast(renderPass)] += subMesh.mVertices.size();
            totalIndices[e_cast(renderPass)] += subMesh.mIndices.size();
        }
    }
    assert(hasSkin != INT32_MAX);

    lock.unlock();

    // Skin data
    if (hasSkin) {
        assert(numMeshes == 1 && "Currently skinned meshes must be a single submesh only");
        const RawSubMesh& baseSubMesh = rawFbxMesh.mSubMeshes[0];
        const int baseMaterialIndex = baseSubMesh.mVertices[0].materialIndex;
        const MaterialRenderPassType baseRenderPass = rawFbxMesh.mMaterials[baseMaterialIndex].materialDef->renderPass;
        rawFbxMesh.mCombinedMeshData[e_cast(baseRenderPass)].mHasSkin = hasSkin;
        rawFbxMesh.mCombinedMeshData[e_cast(baseRenderPass)].mSkeletonData = std::move(rawFbxMesh.mSubMeshes[0].mSkeletonData);
    }
    for (int i = 0; i < e_count(MaterialRenderPassType); ++i) {
        rawFbxMesh.mCombinedMeshData[i].mVertices.resize(totalVertices[i]);
        rawFbxMesh.mCombinedMeshData[i].mIndices.resize(totalIndices[i]);
    }
    // Combine all submeshes by render pass
    int v[e_count(MaterialRenderPassType)] = {};
    int i[e_count(MaterialRenderPassType)] = {};
    int iStart[e_count(MaterialRenderPassType)] = {};
    for (int m = 0; m < numMeshes; ++m) {
        const RawSubMesh& subMesh = rawFbxMesh.mSubMeshes[m];
        const int materialIndex = subMesh.mVertices[0].materialIndex;
        const int renderPassIndex = e_cast(rawFbxMesh.mMaterials[materialIndex].materialDef->renderPass);
        for (int j = 0; j < subMesh.mVertices.size(); ++j) {
            rawFbxMesh.mCombinedMeshData[renderPassIndex].mVertices[v[renderPassIndex]++] = subMesh.mVertices[j];
        }
        for (int j = 0; j < subMesh.mIndices.size(); ++j) {
            rawFbxMesh.mCombinedMeshData[renderPassIndex].mIndices[i[renderPassIndex]++] = subMesh.mIndices[j] + iStart[renderPassIndex];
        }
        iStart[renderPassIndex] += subMesh.mVertices.size();
    }
}

void ModelRepository::onRegisteredAsset(AssetID id) {
    ModelDef& def = *mAssets[id];
    YmlSerializer::readFileData(readFileToString(mAssetRegistry[id].mFilePath), def);
    mLODParameters.resize(mAssets.size());
    updateModelFlyweightData(id);
}

void ModelRepository::onAllAssetTypesRegistered() {
    mLODParameters.shrink_to_fit();
}
void ModelRepository::updateModelFlyweightData(AssetID id) {
    ModelDef& def = *mAssets[id];
    mLODParameters[id].lodDistancesSQ[0] = SQ(def.mLodDistance0);
    mLODParameters[id].lodDistancesSQ[1] = SQ(def.mLodDistance1);
    mLODParameters[id].lodDistancesSQ[2] = SQ(def.mLodDistance2);
    mLODParameters[id].lodDistancesSQ[3] = SQ(def.mLodDistance1);
    mLODParameters[id].boundingSphereRadius = def.mBoundingSphereRadius;
    mLODParameters[id].shadowLodDetail = def.mShadowDetail;
}
