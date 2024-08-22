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
#include "serialization/BitseryExt.h"
#include "RuntimeModelSerializationContext.inl"

static std::mutex gFbxSdkMutex; // FBX SDK IS NOT THREAD SAFE >_<

// TODO: Make false
constexpr bool FORCE_LOAD_FBX = false;

// Debugging
#define FORCE_ONE_AT_A_TIME 0

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
    loadModelDataInternal(def, newName, filePath);
}

void ModelRepository::onAssetChangedByEditor(AssetID id) {
    updateModelFlyweightData(id);
    updateModelVariantData(id);
    updateMaterialDependencies(id);
}

void ModelRepository::loadAllModelData() {
    mUnloadedModelDataCount = mAssetRegistry.size();

    for (AssetID id = 0; id < mAssetRegistry.size(); ++id) {
        ModelDef& def = *mAssets[id];
        LOG_TRACE("Loading model {}", mAssetRegistry[id].mFilePath.getCString());
        if (!def.mModelFileName.isValid()) {
            panic("Model file missing model name - {}", mAssetRegistry[id].mFilePath.getString());
        }

        vio::Path rootDir = mAssetRegistry[id].mFilePath;
        rootDir.trimEnd();
        assert(rootDir.isDirectory());

        const vio::Path modelPath = rootDir + nString("\\") + def.mModelFileName.toString();
        loadModelDataInternal(def, def.getName(), modelPath);
    }
}

void ModelRepository::buildModelBatches() {
    PreciseTimer timer;

    // This function combines all submeshes that are compatible into big batches

    if (mTotalSubmeshCount >= MAX_TOTAL_SUBMESHES) {
        panic("Too many model meshes allocated! Programmer needs to increase maximum submesh count!");
    }
    mVariantArrayIndexData.resize(mAssetRegistry.size());
    mModelSubmeshSpanKeys.resize(mAssetRegistry.size());
    mModelSubmeshCountsPerPass.resize(mAssetRegistry.size());
    mAllSubmeshDrawData.resize(mTotalSubmeshCount);


    std::vector<i32> allSubmeshWindTypes;
    allSubmeshWindTypes.resize(mTotalSubmeshCount);

    struct ModelBatchSubmeshSource {
        MaterialRenderPassType renderPass;
        ModelBatchID batchId;
        i32 startVertex;
        i32 startIndex;
        MeshCpuData* cpuData;
    };

    std::vector<ModelBatchSubmeshSource> submeshSources;
    submeshSources.reserve(mTotalSubmeshCount);

    struct ModelBatchCreationData {
        ui32 verticesSize = 0;
        ui32 indicesSize = 0;
        ModelBatchID batchId = 0;
    };

    FlatMap<ModelBatchKey, ModelBatchCreationData> modelBatchCreationDataMap;
    modelBatchCreationDataMap.reserve(32);

    ui32 numVariantData = 0;

    int totalModelBatches = 0;
    for (AssetID modelId = 0; modelId < mAssetRegistry.size(); ++modelId) {
        ModelDef& def = *mAssets[modelId];

        // Point model to this draw command list
        mModelSubmeshSpanKeys[modelId].index = submeshSources.size();
        mModelSubmeshSpanKeys[modelId].count = def.mSubmeshData.size();

        mVariantArrayIndexData[modelId].offset = numVariantData * MATERIAL_SLOT_COUNT;
        mVariantArrayIndexData[modelId].stride = def.mSubmeshData.size() * MATERIAL_SLOT_COUNT;
        numVariantData += def.mSubmeshData.size() * def.mVariants.size();

        auto& submeshCountArray = mModelSubmeshCountsPerPass[modelId];
        submeshCountArray.fill(0);

        for (size_t submeshIndex = 0; submeshIndex < def.mSubmeshData.size(); ++submeshIndex) {

            MeshCpuData& cpuData = def.mSubmeshCpuData[submeshIndex];
            ModelSubmeshData& submeshData = def.mSubmeshData[submeshIndex];
            ModelBatchKey key;
            key.indexType = cpuData.mIndexType;
            if (key.indexType != MeshIndexType::USHORT) [[unlikely]] {
                // To support UINT, we need ModelRepository to support splitting batches by index type
                panic("ModelRepository::buildModelBatches: Only ushort indices are supported but mesh loaded with more than 65536 vertices");
            }
            key.vertexType = cpuData.mVertexType;
            key.renderPass = submeshData.renderPass;
            ++submeshCountArray[e_cast(key.renderPass)];
            auto&& it = modelBatchCreationDataMap.find(key);
            ModelBatchCreationData* creationData;
            if (it == modelBatchCreationDataMap.end()) {
                creationData = &modelBatchCreationDataMap.emplace(key, ModelBatchCreationData()).first->second;
                creationData->batchId = totalModelBatches++;
            }
            else {
                creationData = &it->second;
            }

            const size_t submeshArrayIndex = submeshSources.size();
            ModelBatchSubmeshSource& submeshSource = submeshSources.emplace_back();
            submeshSource.renderPass = key.renderPass;
            submeshSource.batchId = creationData->batchId;
            submeshSource.startVertex = creationData->verticesSize;
            submeshSource.startIndex = creationData->indicesSize;
            submeshSource.cpuData = &cpuData;

            ModelBatchSubmeshDrawData& drawData = mAllSubmeshDrawData[submeshArrayIndex];
            drawData.batchID = creationData->batchId;
            drawData.renderPass = key.renderPass;
            drawData.castsShadow = submeshData.castsShadow();
            drawData.baseVertex = creationData->verticesSize;

            allSubmeshWindTypes[submeshArrayIndex] = (i32)submeshData.windType;

            for (int l = 0; l < (int)MeshLODLevel::COUNT; ++l) {
                drawData.lodDrawInfo[l] = cpuData.mLodData.getDrawInfoForLOD((MeshLODLevel)l);
                drawData.lodDrawInfo[l].startIndex += submeshSource.startIndex;
            }
            drawData.modelId = modelId;

            creationData->verticesSize += cpuData.mVertsCount;
            creationData->indicesSize += cpuData.mElementsCount;
        }
    }

    // Create all buffer objects and allocate space
    mModelBatches = std::make_unique<ModelBatch[]>(totalModelBatches);
    mModelBatchLookup.reserve(totalModelBatches);
    for (auto& [key, creationData] : modelBatchCreationDataMap) {
        ModelBatch& batch = mModelBatches[creationData.batchId];
        mModelBatchLookup[key] = &batch;
        batch.mRenderPass = key.renderPass;
        batch.mVertexType = key.vertexType;
        batch.mIndexType = key.indexType;
        glCreateVertexArrays(1, &batch.mVao);

        const size_t vertexSize = getVertexSize(key.vertexType);
        batch.mVerticesSizeBytes = creationData.verticesSize * vertexSize;
        glCreateBuffers(1, &batch.mVbo);
        glNamedBufferStorage(batch.mVbo, batch.mVerticesSizeBytes, nullptr, GL_DYNAMIC_STORAGE_BIT);
        glVertexArrayVertexBuffer(batch.mVao, 0, batch.mVbo, 0, vertexSize);

        const size_t indexSize = util::getMeshIndexSizeBytes(key.indexType);
        batch.mIndicesSizeBytes = creationData.indicesSize * indexSize;
        glCreateBuffers(1, &batch.mIbo);
        glNamedBufferStorage(batch.mIbo, batch.mIndicesSizeBytes, nullptr, GL_DYNAMIC_STORAGE_BIT);
        glVertexArrayElementBuffer(batch.mVao, batch.mIbo);

        LOG_INFO("Creating model batch {} {} with {:0.3f} mb vertex and {:0.3f} mb index",
            (int)key.renderPass, creationData.batchId, f32(batch.mVerticesSizeBytes / 1024.0 / 1024.0), f32(batch.mIndicesSizeBytes / 1024.0 / 1024.0));
    }

    // Create variant data UBO
    glCreateBuffers(1, &mModelVariantDataSSBO);
    glNamedBufferStorage(mModelVariantDataSSBO, numVariantData * sizeof(ModelVariantGpuData), nullptr, GL_DYNAMIC_STORAGE_BIT);
    ui32 zero = 0;
    // Zero the buffer (Default material), we will end up uploading materials as they are loaded
    glClearNamedBufferData(mModelVariantDataSSBO, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, &zero);

    glCreateBuffers(1, &mModelSubmeshWindSSBO);
    glNamedBufferStorage(mModelSubmeshWindSSBO, allSubmeshWindTypes.size() * sizeof(ui32), allSubmeshWindTypes.data(), 0);
    
    assert(numVariantData < UINT16_MAX && "If this fails we need to increase variant bits in Vertex.h");

    // Upload all data
    for (ModelBatchSubmeshSource& source : submeshSources) {
        ModelBatch& targetBatch = mModelBatches[source.batchId];
        MeshCpuData& sourceData = *source.cpuData;
        assert(sourceData.mVertexType == targetBatch.mVertexType);
        assert(sourceData.mIndexType == targetBatch.mIndexType);
        const ui32 vertexSize = (ui32)getVertexSize(sourceData.mVertexType);
        const ui32 indexSize = (ui32)util::getMeshIndexSizeBytes(sourceData.mIndexType);
        glNamedBufferSubData(targetBatch.mVbo, source.startVertex * vertexSize, sourceData.mVertsCount * vertexSize, source.cpuData->mVertsPtr);
        glNamedBufferSubData(targetBatch.mIbo, source.startIndex * indexSize, sourceData.mElementsCount * indexSize, source.cpuData->mElementsPtr);
    }

    // Bind vertex attribs
    for (size_t i = 0; i < mModelBatchLookup.size(); ++i) {
        ModelBatch& batch = mModelBatches[i];
        switch (batch.mVertexType) {
            case VertexType::STANDARD_MODEL:
                StandardModelVertex::bindVertexAttribs(batch.mVao);
                break;
            case VertexType::SKINNED_MODEL:
                SkinnedModelVertex::bindVertexAttribs(batch.mVao);
                break;
            default:
                panic("Invalid vertex type {} when building model batches", (int)batch.mVertexType);
        }
    }

    checkGlError("ModelRepository::buildModelBatches");
    LOG_INFO("Built all model batches in {} ms", timer.stop());
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

        updateMaterialDependencies(def.getID());

        // Load material dependencies
        assetLoader.requestAssetLoadWithDependencies(nullptr /* loadFunc*/,
            [this]ASSET_LOAD_LAMBDA(assetId, filePath, assetDataPtr, userData) {

            updateModelVariantData(assetId);
            // TODO: IMPLEMENT THIS
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
    };
}

void ModelRepository::loadModelDataInternal(ModelDef& def, StrToken modelName, const vio::Path& modelPath) {

    // Note that we do not set material dependencies yet. We are not completing load here, we can leave
    // materials unloaded until the asset is actually referenced. The point is to load all data in at startup so we can pack them into
    // shared VBOs. We also dont need to add rigs or animmachines as a dependency because they are guarenteed loaded by the ResourceManager preload
    /*if (def.mRigRef.isValid()) {
        def.addDependency(def.mRigRef.getAssetHandleBase());
        if (def.mMachineRef.isValid()) {
            def.addDependency(def.mMachineRef.getAssetHandleBase());
        }
    }*/

    assert(!def.getDependencies() || !def.getDependencies()->getCount());
    AssetLoader::getInstance().requestAssetLoadWithDependencies([this, modelPath]ASSET_LOAD_LAMBDA(assetId, filePath, assetDataPtr, userData) {
        ModelDef& def = *static_cast<ModelDef*>(assetDataPtr);

#if FORCE_ONE_AT_A_TIME
        static std::mutex sMutex;
        std::lock_guard lock(sMutex);
#endif

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
        if (!FORCE_LOAD_FBX && fs::exists(rnmdlPath)) {
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
            FBXRawModel rawFbxModel;
            FbxLoadContext fbxLoadContext(modelPath.getCString());

            // Default Material dependencies
            const int materialCount = fbxLoadContext.data->sceneLoader.scene()->GetMaterialCount();
            rawFbxModel.mMaterials.resize(materialCount);

            MaterialRepository& materialRepo = MaterialRepository::get();

            for (int i = 0; i < materialCount; ++i) {
                FbxSurfaceMaterial* fbxMaterial = fbxLoadContext.data->sceneLoader.scene()->GetMaterial(i);
                assert(fbxMaterial);
                rawFbxModel.mMaterials[i] = fbx2raw::readFbxMaterial(*fbxMaterial);
                AssetHandlePtr<MaterialDef> materialHandle = materialRepo.getAssetHandle(StrToken(rawFbxModel.mMaterials[i].materialName));
                if (materialHandle) {
                    rawFbxModel.mMaterials[i].defaultMaterialDef = &materialRepo.getLoadedOrUnloadedAsset(materialHandle->getAssetID());
                    def.addDependency(std::move(materialHandle));
                }
            }

            // Load model to raw
            loadRawModelFromFBX(fbxLoadContext, rawFbxModel, filePath, def.mRig ? &def.mRig->mSkeleton : nullptr);
            if (def.mForceNormalsUp) {
                MeshOperations::setAllNormals(rawFbxModel, f32v3(0.0f, 0.0f, 1.0f), f32v3(1.0f, 0.0f, 0.0f));
            }

            f32 minX = FLT_MAX;
            f32 maxX = -FLT_MAX;
            f32 minY = FLT_MAX;
            f32 maxY = -FLT_MAX;
            f32 minZ = FLT_MAX;
            f32 maxZ = -FLT_MAX;

            // TODO: Configure
            const bool shouldCombineMeshes = !rawFbxModel.mHasSkin;
            if (shouldCombineMeshes) {
                combineSubmeshesByRenderPass(rawFbxModel);
            }
            assert(rawFbxModel.mSubMeshes.size());

            std::experimental::fixed_capacity_vector<ui16, 4> rawMaterialIdSlotMapping;

            def.mTotalSubmeshJointTransformsNeeded = 0;

            // TODO: Handle other submeshes?
            int variantMaterialSlotOffset = 0;

            const bool needsConstructDefaultVariant = def.mVariants.empty();
            if (needsConstructDefaultVariant) {
                int submeshCount = 0;
                def.mVariants.resize(1);
            }

            for (auto& [renderPassIndex, subMeshList] : rawFbxModel.mSubMeshes) {
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
                            assert(rawMaterialIdSlotMapping.size() < 4);
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
                            def.mVariants[0].submeshMaterials.back()[i].setAssetName(StrToken(rawFbxModel.mMaterials[rawMaterialIdSlotMapping[i]].materialName));
                        }
                    }

                    MeshCpuData& newMeshCpuData = def.mSubmeshCpuData.emplace_back(
                        ModelMeshBuilder::buildRuntimeOptimizedMeshFromRawMesh(
                            subMesh, rawFbxModel.mMaterials, def.mBaseOptimizeErrorThresold, variantMaterialSlotOffset, &rawMaterialIdSlotMapping
                        )
                    );

                    // All material slots are stored sequentially submesh by submesh in our variant data array
                    variantMaterialSlotOffset += MATERIAL_SLOT_COUNT;

                    // Apply scale if needed
                    if (def.mScale != 1.0f) {
                        MeshOperations::applyScale(newMeshCpuData, def.mScale);
                    }
                    RawMeshSkeletonData& rawSkeletonData = subMesh.mSkeletonData;

                    // Allocate and fill skeleton data
                    if (rawSkeletonData.mNumJoints) {
                        assert(def.mRig && "Missing rig for skeletal model");
                        MeshSkeletonData& skeletonData = def.mSubmeshSkeletonData.emplace_back();
                        skeletonData.mNumJoints = rawSkeletonData.mNumJoints;
                        skeletonData.mJointRemaps = std::unique_ptr<ui8[]>(new ui8[skeletonData.mNumJoints]);
                        memcpy(skeletonData.mJointRemaps.get(), rawSkeletonData.mJointRemaps.data(), sizeof(ui8) * skeletonData.mNumJoints);
                        skeletonData.mInverseBindPoses = std::unique_ptr<ozz::math::Float4x4[]>(new ozz::math::Float4x4[skeletonData.mNumJoints]);
                        memcpy(skeletonData.mInverseBindPoses.get(), rawSkeletonData.mInverseBindPoses.data(), sizeof(ozz::math::Float4x4) * skeletonData.mNumJoints);
                        def.mTotalSubmeshJointTransformsNeeded += skeletonData.mNumJoints;
                    }

                    ModelSubmeshData& newSubmeshData = def.mSubmeshData.emplace_back();
                    newSubmeshData.name = subMesh.mName;
                }
            }

            minX *= def.mScale;
            maxX *= def.mScale;
            minY *= def.mScale;
            maxY *= def.mScale;
            minZ *= def.mScale;
            maxZ *= def.mScale;
            def.mAABB = f32AABB3(f32v3(minX, minY, minZ), f32v3(maxX - minX, maxY - minY, maxZ - minZ));

            def.mSubmeshSkeletonData.shrink_to_fit();
            def.mSubmeshCpuData.shrink_to_fit();
            def.mSubmeshData.shrink_to_fit();

            saveCachedRuntimeModel(def, rnmdlPath);
        }

        mTotalSubmeshCount += def.mSubmeshData.size();
        --mUnloadedModelDataCount;
        return false;
    }, nullptr,
        def.getID(),
        &def,
        modelPath,
        nullptr,
        nullptr,
        def.getDependencies()
    );

}

void ModelRepository::loadRawModelFromFBX(FbxLoadContext& loadContext, FBXRawModel& rawFbxMesh, const vio::Path& filePath, const ozz::animation::Skeleton* skeleton) {
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
        newSubMesh.mName = StrToken(fbxMesh->GetName());
        //LOG_DEBUG("Processing mesh: {} {} deformer count {}", fbxMesh->GetName(), filePath.getString(), fbxMesh->GetDeformerCount(FbxDeformer::eSkin));
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
    rawFbxMesh.mHasSkin = hasSkin;
    assert(hasSkin != INT32_MAX);
    lock.unlock();
}

void ModelRepository::combineSubmeshesByRenderPass(OUT FBXRawModel& rawFbxMesh) {
    assert(!rawFbxMesh.mHasSkin); // Not supported yet, intended for only static props

    std::map<int/*renderPassIndex*/, RawSubMesh> combinedSubMeshes;

    for (auto& [renderPassIndex, subMeshList] : rawFbxMesh.mSubMeshes) {
        if (subMeshList.size() == 1) {
            combinedSubMeshes[(int)renderPassIndex] = std::move(subMeshList[0]);
            continue;
        }
        i32 totalVerts = 0;
        i32 totalInds = 0;
        for (RawSubMesh& subMesh : subMeshList) {
            totalVerts += subMesh.mVertices.size();
            totalInds += subMesh.mIndices.size();
        }

        RawSubMesh& combined = combinedSubMeshes[(int)renderPassIndex];

        combined.mVertices.resize(totalVerts);
        combined.mIndices.resize(totalInds);

        i32 vertexStart = 0;
        i32 indexStart = 0;
        for (RawSubMesh& subMesh : subMeshList) {
            memcpy(&combined.mVertices[vertexStart], &subMesh.mVertices[0], subMesh.mVertices.size() * sizeof(RawMeshVertex));
            for (size_t i = 0; i < subMesh.mIndices.size(); ++i) {
                combined.mIndices[indexStart + i] = vertexStart + subMesh.mIndices[i];
            }
            vertexStart += subMesh.mVertices.size();
            indexStart += subMesh.mIndices.size();
        }
        combined.mRenderPassType = renderPassIndex;
        combined.mHasSkin = false;
    }

    rawFbxMesh.mSubMeshes.clear();
    for (auto& [renderPassIndex, combined] : combinedSubMeshes) {
        rawFbxMesh.mSubMeshes[(MaterialRenderPassType)renderPassIndex].emplace_back(std::move(combined));
    }
}

void ModelRepository::loadCachedRuntimeModel(ModelDef& def, const vio::Path& modelPath, const ozz::animation::Skeleton* skeleton) {
    PreciseTimer timer;
    fs::path stdPath = modelPath.getStdPath();

    BBuffer bbuffer(fs::file_size(stdPath));

    std::ifstream file(stdPath, std::ios::binary);
    if (!file.is_open()) {
        panic("Could not open {} for read", modelPath.getCString());
    }

    file.read(reinterpret_cast<char*>(bbuffer.data()), bbuffer.size());

    RuntimeModelSerializationContext serializeContext(def, modelPath);
    auto state = bitsery::quickDeserialization(BInputAdapter{ bbuffer.data(), bbuffer.size() }, serializeContext);
    if (state.first != bitsery::ReaderError::NoError || !state.second) {
        panic("deserialization error {} on {}", (int)state.first, modelPath.getCString());
    }

    file.close();

    LOG_DEBUG("Loaded {} in {} ms", modelPath.getCString(), timer.stop());
}

void ModelRepository::saveCachedRuntimeModel(ModelDef& def, const vio::Path& modelPath) {

    PreciseTimer timer;

    vio::Path dirPath = modelPath;
    dirPath.trimEnd();

    fs::path stdDirPath = dirPath.getStdPath();
    if (!fs::exists(stdDirPath)) {
        if (!fs::create_directories(stdDirPath)) {
            panic("Failed to create {} directory. Insufficient permissions?", dirPath.getCString());
        }
    }

    std::ofstream file(modelPath.getCString(), std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        panic("Could not open {} for write", modelPath.getCString());
    }

    BBuffer bbuffer;
    bbuffer.reserve(65536); // Arbitrary
    RuntimeModelSerializationContext serializeContext(def, modelPath);
    const ui32 writtenBytes = bitsery::quickSerialization<BOutputAdapter>(bbuffer, serializeContext);

    file.write(reinterpret_cast<const char*>(bbuffer.data()), writtenBytes);
    file.close();

    LOG_DEBUG("Saved {} in {} ms", modelPath.getCString(), timer.stop());
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

    if (def.mVariants.empty()) {
        def.mVariants.resize(1); // Must have a single variant at least
    }
    else if (def.mVariants.size() > MAX_MODEL_VARIANTS) {
        def.mVariants.resize(MAX_MODEL_VARIANTS);
    }

    // Ensure no size mismatch
    for (size_t i = 0; i < def.mVariants.size(); ++i) {
        ModelVariantData& varData = def.mVariants[i];
        varData.submeshMaterials.resize(def.mSubmeshData.size());
    }

    ModelVariantGpuDataContainer variantsGpuData;
    variantsGpuData.resize(def.mSubmeshData.size() * def.mVariants.size());
    VariantIndexData indexData = mVariantArrayIndexData[id];
    assert(def.mSubmeshData.size() == indexData.stride / MATERIAL_SLOT_COUNT);

    for (size_t variantIndex = 0; variantIndex < def.mVariants.size(); ++variantIndex) {
        ModelVariantData& variantData = def.mVariants[variantIndex];

        for (size_t submeshIndex = 0; submeshIndex < def.getNumMeshes(); ++submeshIndex) {
            auto& mats = variantData.submeshMaterials[submeshIndex];
            for (size_t j = 0; j < mats.size(); ++j) {
                variantsGpuData[variantIndex * def.mSubmeshData.size() + submeshIndex].materials[j] = mats[j].getAssetID();
            }
        }
    }

    assert(variantsGpuData.size() == (indexData.stride / MATERIAL_SLOT_COUNT) * def.mVariants.size());
    glNamedBufferSubData(
        mModelVariantDataSSBO,
        indexData.offset * sizeof(ui32) /*VariantIndexData is indexing on individual materials, so 4 bytes*/,
        variantsGpuData.size() * sizeof(ModelVariantGpuData),
        variantsGpuData.data()
    );
    static_assert(sizeof(ModelVariantGpuData) == sizeof(ui32) * MATERIAL_SLOT_COUNT);
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
