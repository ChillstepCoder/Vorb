#include "stdafx.h"
#include "ModelRepository.h"

#include <Vorb/io/IOManager.h>

#include "definitions/AnimMachineDef.h"
#include "resources/RigRepository.h"
#include "resources/AnimMachineRepository.h"
#include "rendering/RenderContext.h"
#include "rendering/mesh/mesher/builder/ModelMeshBuilder.h"
#include "rendering/mesh/MeshOperations.h"
#include "rendering/mesh/Mesh.h"
#include "rendering/model/ModelBillboardLodManager.h"
#include "physics/CollisionShapeRepository.h"

#include "tile/TileDamageData.h"

#include "resources/MaterialRepository.h"
#include "resources/ResourceManager.h"

#include <ozz/base/io/archive.h>
#include <ozz/base/io/stream.h>
#include <ozz/animation/runtime/skeleton.h>

#include "rendering/mesh/fbx2raw.inl"

#include "filesystem/FileSystem.h"

static std::mutex gFbxSdkMutex; // FBX SDK IS NOT THREAD SAFE >_<


struct FbxLoadContextData {
    FbxLoadContextData(const char* filePath) : fbxManager(), settings(fbxManager), sceneLoader(filePath, "", fbxManager, settings) { }
    ozz::animation::offline::fbx::FbxManagerInstance fbxManager;
    ozz::animation::offline::fbx::FbxDefaultIOSettings settings;
    ozz::animation::offline::fbx::FbxSceneLoader sceneLoader;
};

class FbxLoadContext {
public:
    FbxLoadContext(const char* filePath) {
        // FBX SDK IS NOT THREAD SAFE
        gFbxSdkMutex.lock();
        data = std::make_unique<FbxLoadContextData>(filePath);

        if (!data->sceneLoader.scene()) {
            panic("Failed to load fbx file: {}", filePath);
        }
        gFbxSdkMutex.unlock();
    }
    ~FbxLoadContext() {
        if (data) {
            // FBX SDK IS NOT THREAD SAFE
            gFbxSdkMutex.lock();
            data.reset();
            gFbxSdkMutex.unlock();
        }
    }

    std::unique_ptr<FbxLoadContextData> data;
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
    updateModelVariantData(id);
    updateMaterialDependencies(id);
}


AssetLoadFunc ModelRepository::getAssetLoadFunc() {
    return [&]ASSET_LOAD_LAMBDA(assetID, filePath, assetDataPtr) {

        ModelDef& def = *static_cast<ModelDef*>(assetDataPtr);
        //TextureLoadUserData& loadUserData = std::any_cast<TextureLoadUserData&>(userData);

        LOG_TRACE("Loading model {}", filePath.getCString());


        if (!def.mModelFileName.isValid()) {
            panic("Model file missing model name - {}", filePath.getString());
        }

        vio::Path rootDir = filePath;
        rootDir.trimEnd();
        assert(rootDir.isDirectory());

        const vio::Path modelPath = rootDir + nString("\\") + def.mModelFileName.toString();
        loadModelInternal(def, def.getName(), modelPath);

        return false;
    };
}

void ModelRepository::loadModelInternal(ModelDef& def, StrToken modelName, const vio::Path& modelPath) {

    // Allocate raw FBX
    FBXRawMesh* rawMeshPtr;

    {
        std::unique_ptr<FBXRawMesh> rawFbxMesh = std::make_unique<FBXRawMesh>();
        rawMeshPtr = rawFbxMesh.get();
        // TODO: Free this when done!
        std::lock_guard lock(mRawModelsMutex);
        mRawModels[std::move(modelName)] = std::move(rawFbxMesh);
    }

    // If has rig, we need to load animation and skeleton info
    if (def.mRigRef.isValid()) {
        def.addDependency(def.mRigRef.getAssetHandleBase());
        if (def.mMachineRef.isValid()) {
            def.addDependency(def.mMachineRef.getAssetHandleBase());
        }
    }

    AssetLoader::getInstance().requestAssetLoadWithDependencies([this, rawMeshPtr, modelPath]ASSET_LOAD_LAMBDA(assetId, filePath, assetDataPtr, userData) {
        ModelDef& def = *static_cast<ModelDef*>(assetDataPtr);

        
        // Rig + animation
        if (def.mRigRef.isValid()) {
            def.mRig = &def.mRigRef.getLoadedAsset<RigDef>();
            if (def.mMachineRef.isValid()) {
                def.mAnimMachine = &def.mMachineRef.getLoadedAsset<AnimMachineDef>();
            }
        }

        // Runtime model
        fs::path rnmdlPath(filePath.getCString());
        ResourceManager& resourceManager = ResourceManager::get();
        const fs::path& resourceRoot(resourceManager.getResourceRoot().getString());
        const fs::path& cacheRoot(resourceManager.getCacheRoot().getString());
        rnmdlPath.replace_filename(Utils::getFilenameNoExtension(rnmdlPath.string()) + ".rnmdl");
        rnmdlPath = rnmdlPath.lexically_relative(resourceRoot);
        rnmdlPath = cacheRoot / rnmdlPath;

        bool needsLoadFBX = true;
        if (fs::exists(rnmdlPath)) {
            const time_t fileLastWriteTime = FileSystem::getLastFileWriteTime(rnmdlPath);

            fs::path fbxPath(filePath.getStdPath());

            // Find target path
            assert(fs::exists(fbxPath) && fs::is_regular_file(fbxPath));
            if (FileSystem::getLastFileWriteTime(fbxPath) <= fileLastWriteTime) {
                loadCachedRuntimeModel(def, rnmdlPath, def.mRig ? &def.mRig->mSkeleton : nullptr);
                needsLoadFBX = false;
            }
        }

        if (needsLoadFBX) {
            FbxLoadContext fbxLoadContext(modelPath.getCString());

            // Default Material dependencies
            const int materialCount = fbxLoadContext.data->sceneLoader.scene()->GetMaterialCount();
            rawMeshPtr->mMaterials.resize(materialCount);

            const bool needsConstructDefaultVariant = def.mVariants.empty();
            if (needsConstructDefaultVariant) {
                def.mVariants.resize(1);
            }
            else {
                // Variant materials
                updateMaterialDependencies(def.getID());
            }

            MaterialRepository& materialRepo = MaterialRepository::get();

            for (int i = 0; i < materialCount; ++i) {
                FbxSurfaceMaterial* fbxMaterial = fbxLoadContext.data->sceneLoader.scene()->GetMaterial(i);
                assert(fbxMaterial);
                rawMeshPtr->mMaterials[i] = fbx2raw::readFbxMaterial(*fbxMaterial);
                AssetHandlePtr<MaterialDef> materialHandle = materialRepo.getAssetHandle(StrToken(rawMeshPtr->mMaterials[i].materialName));
                if (materialHandle) {
                    rawMeshPtr->mMaterials[i].defaultMaterialDef = &materialRepo.getLoadedOrUnloadedAsset(materialHandle->getAssetID());
                    def.addDependency(std::move(materialHandle));
                }
            }

            // Load model to raw
            loadRawModelFromFBX(fbxLoadContext, *rawMeshPtr, filePath, def.mRig ? &def.mRig->mSkeleton : nullptr);
            if (def.mForceNormalsUp) {
                MeshOperations::setAllNormals(*rawMeshPtr, f32v3(0.0f, 0.0f, 1.0f), f32v3(1.0f, 0.0f, 0.0f));
            }

            f32 minX = FLT_MAX;
            f32 maxX = -FLT_MAX;
            f32 minY = FLT_MAX;
            f32 maxY = -FLT_MAX;
            f32 minZ = FLT_MAX;
            f32 maxZ = -FLT_MAX;

            // TODO: Configure
            const bool shouldCombineMeshes = rawMeshPtr->mHasSkin;
            if (shouldCombineMeshes) {
                combineSubmeshesByRenderPass(*rawMeshPtr);
            }

            std::vector<ui16> rawMaterialIdSlotMapping;
            rawMaterialIdSlotMapping.reserve(4);


            // TODO: Handle other submeshes?
            for (auto& [renderPassIndex, subMeshList] : rawMeshPtr->mSubMeshes) {
                for (RawSubMesh& subMesh : subMeshList) {
                    assert(subMesh.mVertices.size());

                    // Assign material slots and construct AABB
                    rawMaterialIdSlotMapping.clear();

                    for (size_t i = 0; i < subMesh.mVertices.size(); ++i) {
                        RawMeshVertex& rawVert = subMesh.mVertices[i];
                        // Build AABB
                        if (rawVert.pos.x < minX) minX = rawVert.pos.x;
                        if (rawVert.pos.x > maxX) maxX = rawVert.pos.x;
                        if (rawVert.pos.y < minY) minY = rawVert.pos.y;
                        if (rawVert.pos.y > maxY) maxY = rawVert.pos.y;
                        if (rawVert.pos.z < minZ) minZ = rawVert.pos.z;
                        if (rawVert.pos.z > maxZ) maxZ = rawVert.pos.z;

                        // Assign slot index
                        bool foundSlot = false;
                        for (size_t slotIndex = 0; slotIndex < rawMaterialIdSlotMapping.size(); ++slotIndex) {
                            if (rawMaterialIdSlotMapping[slotIndex] == rawVert.rawMaterialIndex) {
                                foundSlot = true;
                                break;
                            }
                        }
                        if (!foundSlot) {
                            rawMaterialIdSlotMapping.push_back(rawVert.rawMaterialIndex);
                        }

                        if (rawVert.pos.z >= 1.0f && rawVert.pos.z <= 2.0f) {

                            // Damage model index binding
                            const float angle = atan2f(rawVert.pos.y, rawVert.pos.x) + M_PIF;
                            int slice = int(floor(angle / (M_2_PIF / MAX_DAMAGE_ZONE_RADIAL_SECTORS)));
                            slice = std::clamp(slice, 0, MAX_DAMAGE_ZONE_RADIAL_SECTORS - 1);
                            rawVert.damageZoneIndex = slice;
                        }
                        else {
                            rawVert.damageZoneIndex = std::numeric_limits<decltype(rawVert.damageZoneIndex)>::max();
                        }
                    }

                    // Assign default materials to slots
                    if (needsConstructDefaultVariant) {
                        def.mVariants[0].submeshMaterials.emplace_back();
                        for (size_t i = 0; i < rawMaterialIdSlotMapping.size(); ++i) {
                            def.mVariants[0].submeshMaterials.back()[i].setAssetName(StrToken(rawMeshPtr->mMaterials[rawMaterialIdSlotMapping[i]].materialName));
                        }
                    }

                    MeshCpuData newMeshCpuData = ModelMeshBuilder::buildRuntimeOptimizedMeshFromRawMesh(subMesh, rawMeshPtr->mMaterials, def.mBaseOptimizeErrorThresold, &rawMaterialIdSlotMapping);

                    // Apply scale if needed
                    if (def.mScale != 1.0f) {
                        MeshOperations::applyScale(newMeshCpuData, def.mScale);
                    }
                    RawMeshSkeletonData& rawSkeletonData = subMesh.mSkeletonData;

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

                    Mesh& newMesh = *def.mMeshes.back();
                    newMesh.mCpuData = std::move(newMeshCpuData);
                    newMesh.setRenderPass((MaterialRenderPassType)renderPassIndex);
                }

                minX *= def.mScale;
                maxX *= def.mScale;
                minY *= def.mScale;
                maxY *= def.mScale;
                minZ *= def.mScale;
                maxZ *= def.mScale;
                def.mAABB = f32AABB3(f32v3(minX, minY, minZ), f32v3(maxX - minX, maxY - minY, maxZ - minZ));


                // Track for efficient gpu upload later
                def.mTotalSubmeshJointTransformsNeeded = 0;
                def.mSubmeshesData.resize(def.getNumMeshes());
                if (def.isSkeletalModel()) {
                    for (ui32 i = 0; i < def.getNumMeshes(); ++i) {
                        def.mMeshes[i]->setSubmeshData(&def.mSubmeshesData[i]);
                        const SkeletalMesh& skeletalMesh = def.getSkeletalMesh(i);
                        const MeshSkeletonData& skelData = skeletalMesh.getSkeletonData();
                        def.mTotalSubmeshJointTransformsNeeded += skelData.mNumJoints;
                    }
                }
                else {
                    for (ui32 i = 0; i < def.getNumMeshes(); ++i) {
                        def.mMeshes[i]->setSubmeshData(&def.mSubmeshesData[i]);
                    }
                }

                saveCachedRuntimeModel(def, rnmdlPath);
            }
        }
        // Load material dependencies
        assetLoader.requestAssetLoadWithDependencies([=](AssetLoader& assetLoader, AssetID assetId, const vio::Path& filePath, void* assetDataPtr, std::any&) -> bool {
            ModelDef& def = *static_cast<ModelDef*>(assetDataPtr);
            return true;
        },
        [this]ASSET_LOAD_LAMBDA(assetId, filePath, assetDataPtr, userData) {
            ModelDef& def = *static_cast<ModelDef*>(assetDataPtr);
            for (auto& mesh : def.mMeshes) {
                ModelMeshBuilder::uploadCpuMeshToGpu(mesh->mCpuData, mesh->mGpuData);
            }

            updateModelVariantData(def.getID());

            RenderContext::getInstance().getModelBillboardLodBuilder().initTextureForModel(assetId);

            return true;
        },
            def.getID(),
            &def,
            modelPath,
            mLoadedAssets[def.getID()].get(),
            nullptr,
            def.getDependencies()
        );
        return false;
    }, nullptr,
        def.getID(),
        &def,
        modelPath,
        mLoadedAssets[def.getID()].get(),
        nullptr,
        def.getDependencies()
    );

}

void ModelRepository::loadRawModelFromFBX(FbxLoadContext& loadContext, FBXRawMesh& rawFbxMesh, const vio::Path& filePath, const ozz::animation::Skeleton* skeleton) {
    MaterialRepository& materialRepo = MaterialRepository::get();
    
    // FBX sdk is not thread safe...
    std::unique_lock lock(gFbxSdkMutex);

    if (!loadContext.data) {
        panic("Failed to import fbx scene: {}", filePath.getString());
    }

    const int numMeshes = loadContext.data->sceneLoader.scene()->GetSrcObjectCount<FbxMesh>();
    if (numMeshes == 0) {
        panic("No mesh to process in this file: {}", filePath.getString());
    }

    // Meshes
    ui32 totalVertices[e_count(MaterialRenderPassType)] = {};
    ui32 totalIndices[e_count(MaterialRenderPassType)] = {};
    int hasSkin = INT32_MAX;
    for (int m = 0; m < numMeshes; ++m) {

        FbxMesh* fbxMesh = loadContext.data->sceneLoader.scene()->GetSrcObject<FbxMesh>(m);
        RawSubMesh newSubMesh;

        LOG_DEBUG("Processing mesh: {} {} deformer count {}", fbxMesh->GetName(), filePath.getString(), fbxMesh->GetDeformerCount(FbxDeformer::eSkin));
        ControlPointsRemap remap;
        if (!fbx2raw::buildRawSubmesh(fbxMesh, loadContext.data->sceneLoader.converter(), &remap, newSubMesh, rawFbxMesh.mMaterials, skeleton == nullptr)) {
            panic("Failed to read submesh for: {}", filePath.getString());
        }

        if (fbxMesh->GetDeformerCount(FbxDeformer::eSkin) > 0) {
            assert(hasSkin == true || hasSkin == INT32_MAX);
            newSubMesh.mHasSkin = true;
            hasSkin = true;
            assert(skeleton && "Needs to have skeleton explicitly passed in");
            if (!fbx2raw::buildSkin(fbxMesh, loadContext.data->sceneLoader.converter(), remap, *skeleton, newSubMesh)) {
                panic("Failed to read skinning data: {}", filePath.getString());
            }
        }
        else {
            assert(hasSkin == false || hasSkin == INT32_MAX);
            newSubMesh.mHasSkin = false;
            hasSkin = false;
        }

        if (newSubMesh.mVertices.size()) {
            const int materialIndex = newSubMesh.mVertices[0].rawMaterialIndex;
            assert(rawFbxMesh.mMaterials[materialIndex].defaultMaterialDef);
            newSubMesh.mRenderPassType = rawFbxMesh.mMaterials[materialIndex].defaultMaterialDef->renderPass;
            rawFbxMesh.mSubMeshes[newSubMesh.mRenderPassType].emplace_back(std::move(newSubMesh));
        }
    }
    assert(hasSkin != INT32_MAX);
    lock.unlock();
}

void ModelRepository::combineSubmeshesByRenderPass(OUT FBXRawMesh& rawFbxMesh) {
    assert(!rawFbxMesh.mHasSkin); // Not supported yet, intended for only static props

    std::map<int/*renderPassIndex*/, RawSubMesh> combinedSubMeshes;

    for (auto& [renderPassIndex, subMeshList] : rawFbxMesh.mSubMeshes) {
        i32 totalVerts = 0;
        i32 totalInds = 0;
        for (RawSubMesh& subMesh : subMeshList) {
            totalVerts += subMesh.mVertices.size();
            totalInds += subMesh.mIndices.size();
        }

        RawSubMesh& combined = combinedSubMeshes[(int)renderPassIndex];

        combined.mVertices.reserve(totalVerts);
        combined.mIndices.reserve(totalInds);

        for (RawSubMesh& subMesh : subMeshList) {
            combined.mVertices.insert(combined.mVertices.end(), subMesh.mVertices.begin(), subMesh.mVertices.end());
            combined.mIndices.insert(combined.mIndices.end(), subMesh.mIndices.begin(), subMesh.mIndices.end());
        }
        combined.mRenderPassType = renderPassIndex;
        combined.mHasSkin = false;
    }
}

void ModelRepository::loadCachedRuntimeModel(ModelDef& def, const vio::Path& modelPath, const ozz::animation::Skeleton* skeleton) {

}

void ModelRepository::saveCachedRuntimeModel(ModelDef& def, const vio::Path& modelPath)
{

}

void ModelRepository::onRegisteredAsset(AssetID id) {
    ModelDef& def = *mAssets[id];
    YmlSerializer::readFileData(readFileToString(mAssetRegistry[id].mFilePath), def);
    mLODParameters.resize(mAssets.size());
    updateModelFlyweightData(id);
    updateModelCollision(id);
}

void ModelRepository::onAllAssetTypesRegistered() {
    mLODParameters.shrink_to_fit();
}

void ModelRepository::updateModelFlyweightData(AssetID id) {
    ModelDef& def = *mAssets[id];
    mLODParameters[id].lodDistancesSQ[0] = SQ(def.mLodDistance0);
    mLODParameters[id].lodDistancesSQ[1] = SQ(def.mLodDistance1);
    mLODParameters[id].lodDistancesSQ[2] = SQ(def.mLodDistance2);
    mLODParameters[id].lodDistancesSQ[3] = SQ(def.mLodDistance3);
    mLODParameters[id].boundingSphereRadius = def.mBoundingSphereRadius;
    mLODParameters[id].shadowLodDetail = def.mShadowDetail;
}

void ModelRepository::updateModelVariantData(AssetID id) {
    ASSERT_RENDER_THREAD();

    ModelDef& def = *mAssets[id];
    def.mVariantsGpuData.resize(def.mSubmeshesData.size());
    def.mVariantsGpuBuffers.resize(def.mSubmeshesData.size());

    if (def.mVariants.empty()) {
        def.mVariants.resize(1); // Must have a single variant at least
    }
    else if (def.mVariants.size() > MAX_MODEL_VARIANTS) {
        def.mVariants.resize(MAX_MODEL_VARIANTS);
    }

    // Ensure no size mismatch
    for (size_t i = 0; i < def.mVariants.size(); ++i) {
        ModelVariantData& varData = def.mVariants[i];
        varData.submeshMaterials.resize(def.mSubmeshesData.size());
    }

    // Copy all variant materials to GPU data and then upload
    MaterialRepository& materialRepo = MaterialRepository::get();
    for (size_t submeshIndex = 0; submeshIndex < def.mVariantsGpuData.size(); ++submeshIndex) {
        ModelVariantGpuDataContainer& gpuData = def.mVariantsGpuData[submeshIndex];
        gpuData.resize(def.mVariants.size());

        for (size_t variantIndex = 0; variantIndex < def.mVariants.size(); ++variantIndex) {
            ModelVariantData& variantData = def.mVariants[variantIndex];

            auto& mats = variantData.submeshMaterials[submeshIndex];
            for (size_t j = 0; j < mats.size(); ++j) {
                gpuData[variantIndex].materials[j] = mats[j].getAssetID();
            }
        }

        assert(submeshIndex < def.getNumMeshes());

        // Upload
        GLBuffer& buffer = def.mVariantsGpuBuffers[submeshIndex];
        buffer.allocate(gpuData.size() * sizeof(ModelVariantGpuData), gpuData.data(), 0);
        def.mMeshes[submeshIndex]->mVariantDataUbo = buffer.getHandle();
    }
}

void ModelRepository::updateMaterialDependencies(AssetID id) {
    ModelDef& def = *mAssets[id];
    MaterialRepository& materialRepo = MaterialRepository::get();
    for (auto& variant : def.mVariants) {
        for (auto& mats : variant.submeshMaterials) {
            for (MaterialAssetRef matRef : mats) {
                if (matRef.isValid()) {
                    AssetHandlePtr<MaterialDef> assetHandle = matRef.getAssetHandle<MaterialDef>();
                    if (assetHandle) {
                        def.addDependency(std::move(assetHandle));
                    }
                }
            }
        }
    }
}

void ModelRepository::updateModelCollision(AssetID id) {
    ModelDef& def = *mAssets[id];
    if (def.mColliderData.mSubShapes.size() == 1) {
        // Simple single shape
        const ModelColliderShape& shape = def.mColliderData.mSubShapes[0];
       
        if (shape.mShape != CollisionShapes::NONE) {
            def.mCollisionShapeID = sCollisionShapeRepository->getOrAddCollisionShape(shape.mShape, shape.mHalfDims);

            def.mColliderData.mBaseOrientation = glm::quat(shape.mEulerAngles);
            if (def.mColliderData.mBaseOrientation != glm::quat(1.0f, 0.0f, 0.0f, 0.0f)) {
                def.mColliderData.mInverseBaseOrientation = glm::inverse(def.mColliderData.mBaseOrientation);
                def.mColliderData.mHasBaseOrientation = true;
            }
            if (shape.mOffset != f32v3(0.0f)) {
                def.mColliderData.mBaseOffset = shape.mOffset;
                def.mColliderData.mHasBaseOffset = true;
            }
        }
    }
    else if (def.mColliderData.mSubShapes.size() > 1) {
        // Compound shape
        def.mColliderData.mBaseOrientation = def.mColliderData.mInverseBaseOrientation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        def.mColliderData.mHasBaseOrientation = false;
        def.mColliderData.mHasBaseOffset = false;
        def.mCollisionShapeID = sCollisionShapeRepository->addCompoundCollisionShape(def.mColliderData.mSubShapes);
    }
    else {
        // No shape
        def.mColliderData.mBaseOrientation = def.mColliderData.mInverseBaseOrientation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        def.mCollisionShapeID = INVALID_COLLISION_SHAPE_ID;
    }
}
