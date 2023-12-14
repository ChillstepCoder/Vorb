#include "stdafx.h"
#include "InstancedStaticModelManager.h"

#include "resources/ResourceManager.h"
#include "resources/ModelRepository.h"
#include "resources/TileRepository.h"
#include "tile/TileContainer.h"

#include "rendering/MaterialShaderRepository.h"
#include "rendering/model/InstancedStaticModelGatherer.h"
#include "rendering/model/ModelUtil.h"
#include "rendering/mesh/mesher/builder/ModelMeshBuilder.h"
#include "rendering/RenderThreadTasks.h"
#include "rendering/mesh/mesher/builder/TileMeshBuilderMethods.h"

#include "camera/Camera3D.h"

#include <boost/pool/singleton_pool.hpp>

#include "options/DebugOptions.h"

#include "rendering/gl/GL.h"

// Types of events we handle
constexpr ui8 MODEL_EDIT_HANDLE_MASK = e_cast(TileContainerEditEventType::ChangeZPos) | e_cast(TileContainerEditEventType::ChangeLayer) | e_cast(TileContainerEditEventType::ChangeOrientation) | e_cast(TileContainerEditEventType::ChangeZPos);
static_assert(e_cast(TileContainerEditEventType::TYPES) == 5, "Update handler");

struct TileContainerModelEditEvent {

    void* operator new(size_t count);
    void operator delete(void* pointer, size_t size);

    TileContainerID containerId;
    TileContainerEditEvent editEvent = {};
};

struct edit_task_pool {};
using edit_singleton_task_pool = boost::singleton_pool<edit_task_pool, sizeof(TileContainerModelEditEvent), boost::default_user_allocator_new_delete, boost::details::pool::default_mutex, 256u>;

void* TileContainerModelEditEvent::operator new(size_t count) {
    UNUSED(count);
    return edit_singleton_task_pool::malloc();
}

void TileContainerModelEditEvent::operator delete(void* pointer, size_t size) {
    UNUSED(size);
    return edit_singleton_task_pool::free(pointer);
}

constexpr int WORK_GROUP_SIZE = 64;
// TODO: Read about advanced gpu driven rendering https://advances.realtimerendering.com/s2015/aaltonenhaar_siggraph2015_combined_final_footer_220dpi.pdf

// Must be done or we will corrupt gpu memory :P
size_t roundToWorkGroupSize(size_t in) {
    size_t remainder = in % WORK_GROUP_SIZE;
    if (remainder == 0) {
        return in;
    }
    return in + (WORK_GROUP_SIZE - remainder);
}
#pragma pack(push, 1)
struct PACKED_STRUCT GpuCullUniformData {
    f32v4 frustumPlanes[4];
    MeshLODDrawInfo lodDrawInfos[4];
    f32v4 cameraPos;
    float lodDistancesSQ[4];
    ui32 numShapesToCull;
};
#pragma pack(pop)
static_assert(offsetof(GpuCullUniformData, numShapesToCull) == 128);
static_assert(sizeof(GpuCullUniformData) == 132);
static_assert(sizeof(MeshLODDrawInfo) == sizeof(ui32v2));

InstancedStaticModelManager::InstancedStaticModelManager() :
    mGpuCullingUniformBuffer(sizeof(GpuCullUniformData), nullptr, GL_DYNAMIC_STORAGE_BIT)
{
    mCullingComputeShader = MaterialShaderRepository::get().getAssetHandle(CStrToken("culling_and_lod"));
}

InstancedStaticModelManager::~InstancedStaticModelManager() {
    for (int ri = 0; ri < e_cast(MaterialRenderPassType::COUNT); ++ri) {
        for (auto& it : mModelsToInstances[ri]) {
            GL.glDeleteBuffers(1, &it.second.mTransformsVbo);
        }
    }
}

void InstancedStaticModelManager::frameUpdate(const Camera3D& camera, f32 elapsedSec)
{
    ASSERT_RENDER_THREAD();
    if (sDebugOptions.mHideModels)
        return;

    PROFILE_FUNCTION();

    updatePendingModelDefs();

    // Build GPU data and cull
    for (int ri = 0; ri < e_cast(MaterialRenderPassType::COUNT); ++ri) {
        for (auto& it : mModelsToInstances[ri]) {
            StaticMeshInstanceData& instanceData = it.second;
            // TODO: Move this to onRemove
            if (!instanceData.mInstanceTransforms.size()) {
                instanceData.mDrawCommands.reset();
                instanceData.mDrawCommandsShadows.reset();
                if (instanceData.mTransformsVbo) {
                    GL.glDeleteBuffers(1, &instanceData.mTransformsVbo);
                    instanceData.mTransformsVbo = 0;
                }
                continue;
            }
            assert(instanceData.mMesh);

            ModelID modelId = it.first;
            const ModelLodParams& lodParams = ModelRepository::get().getLodParams(modelId);
            const Mesh& mesh = *instanceData.mMesh;
            MeshLODDrawInfo drawInfos[4];
            for (int i = 0; i < 4; ++i) {
                drawInfos[i] = mesh.mGpuData.mLODData.getDrawInfoForLOD(MeshLODLevel(i));
            }
            if (instanceData.mFirstDirtyInstance != UINT32_MAX) {
                PROFILE_SCOPE("Rebuild Indirect Buffer");
                const size_t workGroupRoundedSize = roundToWorkGroupSize(instanceData.mInstanceTransforms.size());
                assert(workGroupRoundedSize >= instanceData.mInstanceTransforms.size());
                // Rebuild command buffer
                {
                    PROFILE_SCOPE("Indirect Buffer");
                    instanceData.mDrawCommands = std::make_unique<GLDrawCommandBuffer>(workGroupRoundedSize);
                    instanceData.mDrawCommandsShadows = std::make_unique<GLDrawCommandBuffer>(workGroupRoundedSize);
                }

                // Allocate VBO
                {
                    PROFILE_SCOPE("VBO");
                    // GPU buffer is larger to accomidate the work group size, or we get corruption
                    const GLsizei gpuBufferSizeBytes = sizeof(f32m4) * workGroupRoundedSize;
                    const GLsizei cpuBufferSizeBytes = sizeof(f32m4) * instanceData.mInstanceTransforms.size();
                    // Transform takes up 4 binding points
                    if (instanceData.mTransformsVbo == 0) {
                        GL.glCreateBuffers(1, &instanceData.mTransformsVbo);
                        mesh.bindModelTransformAttribs();
                        GL.glNamedBufferStorage(instanceData.mTransformsVbo, gpuBufferSizeBytes, nullptr, GL_DYNAMIC_STORAGE_BIT);
                        GL.glNamedBufferSubData(instanceData.mTransformsVbo, 0, cpuBufferSizeBytes, instanceData.mInstanceTransforms.data());
                        instanceData.mTransformsVboSizeBytes = gpuBufferSizeBytes;
                    }
                    else if (gpuBufferSizeBytes > instanceData.mTransformsVboSizeBytes) {
                        //LOG_INFO("GROW {} {}", cpuBufferSizeBytes, gpuBufferSizeBytes);
                        // Grow to new size
                        GL.glDeleteBuffers(1, &instanceData.mTransformsVbo);
                        GL.glCreateBuffers(1, &instanceData.mTransformsVbo);
                        GL.glNamedBufferStorage(instanceData.mTransformsVbo, gpuBufferSizeBytes, nullptr, GL_DYNAMIC_STORAGE_BIT);
                        GL.glNamedBufferSubData(instanceData.mTransformsVbo, 0, cpuBufferSizeBytes, instanceData.mInstanceTransforms.data());
                        instanceData.mTransformsVboSizeBytes = gpuBufferSizeBytes;
                    }
                    else {
                        //LOG_INFO("SHRINK {} {}  {} {}", instanceData.mFirstDirtyInstance, instanceData.mInstanceTransforms.size(), cpuBufferSizeBytes, gpuBufferSizeBytes);
                        // Only upload data after the first dirty instance, which should amortize things a bit
                        glNamedBufferSubData(
                            instanceData.mTransformsVbo,
                            instanceData.mFirstDirtyInstance * sizeof(f32m4),
                            cpuBufferSizeBytes - instanceData.mFirstDirtyInstance * sizeof(f32m4),
                            instanceData.mInstanceTransforms.data() + instanceData.mFirstDirtyInstance
                        );
                    }
                }

                instanceData.mFirstDirtyInstance = UINT32_MAX;
            }

            GLDrawCommandBuffer& inDrawCommands = *instanceData.mDrawCommands;
            GLDrawCommandBuffer& inDrawCommandsShadows = *instanceData.mDrawCommandsShadows;
            const size_t drawCommandsCapacity = inDrawCommands.getCapacity();

            inDrawCommands.frameBegin();
            inDrawCommandsShadows.frameBegin();

            if (sDebugOptions.mDisableGPUCulling == false) {
                PROFILE_SCOPE("GPU Culling");
                // GPU Culling
                GpuCullUniformData uniformData;
                const f32v3& camPos = camera.getPosition();
                uniformData.cameraPos = f32v4(camPos.x, camPos.y, camPos.z, 1.0f);
                uniformData.numShapesToCull = drawCommandsCapacity;
                if (sDebugOptions.mDisableLOD) {
                    uniformData.lodDistancesSQ[0] = FLT_MAX;
                }
                else {
                    for (int i = 0; i < 4; ++i) {
                        uniformData.lodDistancesSQ[i] = lodParams.lodDistancesSQ[i];
                    }
                }
                for (int i = 0; i < 4; ++i) {
                    uniformData.frustumPlanes[i] = camera.getFrustum().getPlane(i).vec4Data;
                    uniformData.lodDrawInfos[i] = mesh.mGpuData.mLODData.getDrawInfoForLOD(MeshLODLevel(i));
                }

                mGpuCullingUniformBuffer.updateSubData(0, sizeof(GpuCullUniformData), &uniformData);
                //*instanceData.mNumVisibleMeshesBufferPtr = 0; // Compact indirect buffer is actually slower due to atomic operation and cpu-gpu sync

                if (const MaterialShaderDef* def = mCullingComputeShader->tryGetLoadedAsset()) {
                    def->useCompute();
                    glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
                    GL.glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, instanceData.mTransformsVbo);
                    GL.glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, inDrawCommands.getHandle());
                    GL.glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, inDrawCommandsShadows.getHandle());
                    GL.glBindBufferBase(GL_UNIFORM_BUFFER, 5, mGpuCullingUniformBuffer.getHandle());
                    //glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, instanceData.mNumVisibleMeshesBuffer.getHandle());
                    //glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, outDrawCommands.getHandle()); // Compact indirect buffer is actually slower due to atomic operation and cpu-gpu sync
                    if (drawCommandsCapacity % WORK_GROUP_SIZE == 0) {
                        glDispatchCompute((GLuint)drawCommandsCapacity / WORK_GROUP_SIZE, 1, 1);
                    }
                    else {
                        glDispatchCompute(1 + (GLuint)drawCommandsCapacity / WORK_GROUP_SIZE, 1, 1);
                    }
                    glMemoryBarrier(GL_COMMAND_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT); // GL_ATOMIC_COUNTER_BARRIER_BIT

                    inDrawCommands.setNumActiveCommands(drawCommandsCapacity);
                    inDrawCommandsShadows.setNumActiveCommands(drawCommandsCapacity);
                }
            }
            else {
                assert(instanceData.mInstanceTransforms.size() <= drawCommandsCapacity);
                PROFILE_SCOPE("CPU Culling");
                // CPU Culling
                // TODO: Should the renderer handle this??
                int activeCount = 0;
                int shadowCount = 0;
                for (size_t i = 0; i < instanceData.mInstanceTransforms.size(); ++i) {
                    DrawElementsIndirectCommand& cmd = inDrawCommands.getDrawCommands().data()[activeCount];
                    DrawElementsIndirectCommand& cmdShadow = inDrawCommandsShadows.getDrawCommands().data()[shadowCount];
                    const f32m4& transform = instanceData.mInstanceTransforms[i];
                    // Columns are first
                    const f32v3& pos = reinterpret_cast<const f32v3&>(transform[3]);
                    // TODO: Real bounding sphere
                    if (camera.sphereIsVisible(pos, lodParams.boundingSphereRadius)) {
                        cmd.instanceCount_ = 1;
                        cmdShadow.instanceCount_ = 1;
                        cmd.baseInstance_ = i;
                        cmdShadow.baseInstance_ = i;
                        cmd.baseVertex_ = 0;
                        cmdShadow.baseVertex_ = 0;
                        MeshLODDrawInfo drawInfo;
                        f32 distance2 = glm::length2(pos - camera.getPosition());
                        if ((distance2 < lodParams.lodDistancesSQ[0]) || sDebugOptions.mDisableLOD) {
                            drawInfo = drawInfos[0];
                            if (lodParams.shadowLodDetail > ShadowModelDetail::None) {
                                cmdShadow.count_ = drawInfos[1].indexCount;
                                cmdShadow.firstIndex_ = drawInfos[1].startIndex;
                                ++shadowCount;
                            }
                        }
                        else if (distance2 < lodParams.lodDistancesSQ[1]) {
                            drawInfo = drawInfos[1];
                            if (lodParams.shadowLodDetail > ShadowModelDetail::Low) {
                                cmdShadow.count_ = drawInfos[2].indexCount;
                                cmdShadow.firstIndex_ = drawInfos[2].startIndex;
                                ++shadowCount;
                            }
                        }
                        else if (distance2 < lodParams.lodDistancesSQ[2]) {
                            drawInfo = drawInfos[2];
                            if (lodParams.shadowLodDetail > ShadowModelDetail::Medium) {
                                cmdShadow.count_ = drawInfos[3].indexCount;
                                cmdShadow.firstIndex_ = drawInfos[3].startIndex;
                                ++shadowCount;
                            }
                        }
                        else if (distance2 < lodParams.lodDistancesSQ[3]) {
                            drawInfo = drawInfos[3];
                            if (lodParams.shadowLodDetail == ShadowModelDetail::High) {
                                cmdShadow.count_ = drawInfos[3].indexCount; // ?
                                cmdShadow.firstIndex_ = drawInfos[3].startIndex;
                                ++shadowCount;
                            }
                        }
                        else {
                            // TODO: Billboard
                            continue;
                        }
                        cmd.count_ = drawInfo.indexCount;
                        cmd.firstIndex_ = drawInfo.startIndex;
                        ++activeCount;
                    }
                }
                inDrawCommands.setNumActiveCommands(activeCount);
                inDrawCommandsShadows.setNumActiveCommands(shadowCount);
                inDrawCommands.uploadDrawCommands();
                inDrawCommandsShadows.uploadDrawCommands();

            }

            // Compact indirect buffer is actually slower due to atomic operation and cpu-gpu sync
            // Sync start of draw
           /* if (inDrawCommands.mDrawCommands.size()) {
                glMemoryBarrier(GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT);
                instanceData.mFenceSync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
            }*/

        }
    }

    updateAnimatedModels(elapsedSec);
}

void InstancedStaticModelManager::addInstanceAtPosition(TileContainerID containerId, TileIndex tileIndex, ModelID modelId, f32v3 position, f32 rotation) {
    ASSERT_RENDER_THREAD();

    AssetHandle<ModelDef>* assetHandle;
    auto&& mit = mModelDefRefs.find(modelId);
    if (mit == mModelDefRefs.end()) {
        ModelDefRef newRef;
        newRef.handle = ModelRepository::get().getAssetHandle(modelId);
        assetHandle = newRef.handle.get();
        mModelDefRefs.emplace(std::make_pair(modelId, std::move(newRef)));
    }
    else {
        assetHandle = mit->second.handle.get();
        ++mit->second.refCount;
    }
    
    if (!assetHandle->isLoaded()) {
        mPendingInstances[modelId].emplace_back(PendingModelInstance{ containerId, tileIndex, ModelUtil::computeTransformMatrixForModel(position, rotation) });
        mPendingInstanceForContainer[containerId].emplace(modelId);
        return;
    }
    const ModelDef* modelDefPtr = ModelRepository::get().tryGetLoadedAsset(modelId);
    assert(modelDefPtr);

    addInstanceAtPositionInternal(*modelDefPtr, containerId, tileIndex, ModelUtil::computeTransformMatrixForModel(position, rotation));
}

void InstancedStaticModelManager::removeInstanceAtPosition(TileContainerID containerId, TileIndex tileIndex) {
    ASSERT_RENDER_THREAD();

    // If it is pending, just remove here
    // Remove any asset pending instances
    auto&& pit = mPendingInstanceForContainer.find(containerId);
    if (pit != mPendingInstanceForContainer.end()) {
        for (ModelID id : pit->second) {
            auto&& mit = mPendingInstances.find(id);
            if (mit != mPendingInstances.end()) {
                std::vector<PendingModelInstance>& instances = mit->second;
                // Linear removal, should be rare so its fine
                for (size_t i = 0; i < instances.size();) {
                    if (instances[i].containerId == containerId && instances[i].tileIndex == tileIndex) {
                        instances[i] = instances.back();
                        instances.pop_back();
                        decrefModelDef(id, 1);
                        if (instances.empty()) {
                            mPendingInstances.erase(mit);
                        }
                        // We will not try to erase from mPendingInstanceForContainer here because there may be
                        // other models pending for this container. That is OK

                        // There can only be one
                        return;
                    }
                    else {
                        ++i;
                    }
                }
            }
        }
        mPendingInstanceForContainer.erase(pit);
    }

    for (int renderPassIndex = 0; renderPassIndex < e_count(MaterialRenderPassType); ++renderPassIndex) {
        auto&& it = mTileContainerModels[renderPassIndex].find(containerId);
        if (it != mTileContainerModels[renderPassIndex].end()) {
            TileModelPositionKey key{ tileIndex };
            SpatialInstanceDataMap& spatialMap = it->second;
            auto&& spit = spatialMap.find(key);

            if (spit != spatialMap.end()) {
                removeTileModelInstanceInternal(renderPassIndex, spit->second);
                spatialMap.erase(spit);
                if (spatialMap.empty()) {
                    mTileContainerModels[renderPassIndex].erase(it);
                }
            }
        }
    }
}

bool InstancedStaticModelManager::getInstancesAtPosition(LiteTileHandle tileHandle, OUT TileModelInstance* outInstances[e_cast(MaterialRenderPassType::COUNT)]) {
    ASSERT_RENDER_THREAD();
    bool has = false;
    for (int renderPassIndex = 0; renderPassIndex < e_count(MaterialRenderPassType); ++renderPassIndex) {
        outInstances[renderPassIndex] = nullptr;
        auto&& it = mTileContainerModels[renderPassIndex].find(tileHandle.containerId);
        if (it != mTileContainerModels[renderPassIndex].end()) {
            TileModelPositionKey key{ tileHandle.index };
            SpatialInstanceDataMap& spatialMap = it->second;
            auto&& spit = spatialMap.find(key);

            if (spit != spatialMap.end()) {
                outInstances[renderPassIndex] = &spit->second;
                has = true;
            }
        }
    }
    return has;
}

bool InstancedStaticModelManager::hasInstanceAtPosition(LiteTileHandle tileHandle)
{
    for (int renderPassIndex = 0; renderPassIndex < e_count(MaterialRenderPassType); ++renderPassIndex) {
        auto&& it = mTileContainerModels[renderPassIndex].find(tileHandle.containerId);
        if (it != mTileContainerModels[renderPassIndex].end()) {
            TileModelPositionKey key{ tileHandle.index };
            SpatialInstanceDataMap& spatialMap = it->second;
            auto&& spit = spatialMap.find(key);

            if (spit != spatialMap.end()) {
                return true;
            }
        }
    }
    return false;
}

void InstancedStaticModelManager::addInstancesFromGatherer(InstancedStaticModelGatherer& gatherer)
{
    ASSERT_RENDER_THREAD();
    PROFILE_FUNCTION();
    if (gatherer.mInstances.empty()) {
        return;
    }
    // Remove all instances before we add new ones
    removeInstancesFromContainer(gatherer.mContainerID);

    for (auto&& it : gatherer.mInstances) {

        ModelID modelId = it.first;
        const std::vector<StaticModelInstance>& sourceInstances = it.second;

        AssetHandle<ModelDef>* assetHandle;
        auto&& mit = mModelDefRefs.find(modelId);
        if (mit == mModelDefRefs.end()) {
            ModelDefRef newRef;
            newRef.handle = ModelRepository::get().getAssetHandle(modelId);
            newRef.refCount = sourceInstances.size();
            assetHandle = newRef.handle.get();
            mModelDefRefs.emplace(std::make_pair(modelId, std::move(newRef)));
        }
        else {
            assetHandle = mit->second.handle.get();
            mit->second.refCount += sourceInstances.size();
        }

        if (!assetHandle->isLoaded()) {
            std::vector<PendingModelInstance>& pending = mPendingInstances[modelId];
            pending.reserve(pending.size() + sourceInstances.size());
            for (size_t i = 0; i < sourceInstances.size(); ++i) {
                const StaticModelInstance& modelInstance = sourceInstances[i];
                pending.emplace_back(PendingModelInstance{ gatherer.mContainerID, modelInstance.tileIndex, modelInstance.matrix });
            }
            mPendingInstanceForContainer[gatherer.mContainerID].emplace(modelId);

            continue;
        }

        // Insert all instance transforms ordered into the transforms array
        const ModelDef* modelDefPtr = ModelRepository::get().tryGetLoadedAsset(modelId);
        assert(modelDefPtr);
        for (int m = 0; m < modelDefPtr->getNumMeshes(); ++m) {
            const Mesh& mesh = modelDefPtr->getMesh(m);
            const int renderPassIndex = e_cast(mesh.getRenderPass());
            SpatialInstanceDataMap& tileContainerModels = mTileContainerModels[renderPassIndex][gatherer.mContainerID];
            StaticMeshInstanceData& instanceData = mModelsToInstances[renderPassIndex][modelId];
            const size_t startIndex = instanceData.mInstanceTransforms.size();
            // Track where our buffer is dirty
            if (startIndex < instanceData.mFirstDirtyInstance) {
                instanceData.mFirstDirtyInstance = startIndex;
            }
            instanceData.mInstanceTransforms.resize(startIndex + sourceInstances.size());
            instanceData.mInstanceOwners.resize(instanceData.mInstanceTransforms.size());
            instanceData.mMesh = &mesh;
            // Store per tile references
            for (size_t i = 0; i < sourceInstances.size(); ++i) {
                size_t instanceIndex = startIndex + i;
                const StaticModelInstance& modelInstance = sourceInstances[i];
                instanceData.mInstanceTransforms[instanceIndex] = modelInstance.matrix;
                instanceData.mInstanceOwners[instanceIndex] = ModelInstanceOwner{ gatherer.mContainerID, modelInstance.tileIndex };
                TileModelPositionKey positionKey{ modelInstance.tileIndex };
                assert(tileContainerModels.find(positionKey) == tileContainerModels.end());
                tileContainerModels[positionKey] = { it.first, (ui32)instanceIndex };
            }
        }
    }
}

void InstancedStaticModelManager::removeInstancesFromContainer(TileContainerID containerId)
{
    // TODO: There is a race condition if the tile container is being meshed. Make sure we only destroy tile containers once they are done
    // being meshed?
    ASSERT_RENDER_THREAD();

    // Remove any asset pending instances
    auto&& pit = mPendingInstanceForContainer.find(containerId);
    if (pit != mPendingInstanceForContainer.end()) {
        for (ModelID id : pit->second) {
            auto&& mit = mPendingInstances.find(id);
            if (mit != mPendingInstances.end()) {
                int removedCount = 0;
                std::vector<PendingModelInstance>& instances = mit->second;
                // Linear removal, should be rare so its fine
                for (size_t i = 0; i < instances.size();) {
                    if (instances[i].containerId == containerId) {
                        instances[i] = instances.back();
                        instances.pop_back();
                        ++removedCount;
                    }
                    else {
                        ++i;
                    }
                }
                decrefModelDef(id, removedCount);
                if (instances.empty()) {
                    mPendingInstances.erase(mit);
                }
            }
        }
        mPendingInstanceForContainer.erase(pit);
    }

    for (int renderPassIndex = 0; renderPassIndex < e_count(MaterialRenderPassType); ++renderPassIndex) {
        auto&& it = mTileContainerModels[renderPassIndex].find(containerId);
        if (it == mTileContainerModels[renderPassIndex].end()) {
            continue;
        }
        SpatialInstanceDataMap& tileContainerModels = it->second;
        for (auto& it2 : tileContainerModels) {
            removeTileModelInstanceInternal(renderPassIndex, it2.second);
        }
        mTileContainerModels[renderPassIndex].erase(it);
    }
}

ui32 InstancedStaticModelManager::getNumModels() const
{
    ASSERT_RENDER_THREAD();
    ui32 numModels = 0;
    for (int ri = 0; ri < e_cast(MaterialRenderPassType::COUNT); ++ri) {
        for (auto& it : mModelsToInstances[ri]) {
            numModels += it.second.mInstanceTransforms.size();
        }
    }
    return numModels;
}

void InstancedStaticModelManager::playAnimationOnInstanceAtPosition(LiteTileHandle targetTile, StaticModelAnimationTypes animType, f32v2 direction) {
    // Ensure tile exists as static model
    if (!hasInstanceAtPosition(targetTile)) {
        return;
    }

    // Overwrite existing animation if any
    StaticMeshAnimation& anim = mAnimatedInstances[targetTile];
    anim.animType = animType;
    anim.currentTimeSec = 0.0f;
    anim.direction = direction;
}

void InstancedStaticModelManager::onContainerEditEvent(const TileContainerEvent& evnt) {
    ASSERT_GAME_THREAD();

    const TileContainerEditEvent& editEvent = std::get<TileContainerEditEvent>(evnt.varEvent);

    if ((e_cast(editEvent.type) & MODEL_EDIT_HANDLE_MASK) == 0) {
        return;
    }

    struct ModelAddEvent {
        f32v3 worldPosition;
        TileIndex tileIndex;
        ModelID modelId;
    };
    struct ModelEditEvents {
        TileContainerID containerId;
        InstancedStaticModelManager* manager = nullptr;
        std::vector<ModelAddEvent> addEvents;
        std::vector<TileIndex> removeEvents;
    };

    ModelEditEvents editEvents;
    editEvents.manager = this;
    editEvents.containerId = evnt.container->getId();

    switch (editEvent.type) {
        case TileContainerEditEventType::ChangeFlags:
            break;
        case TileContainerEditEventType::ChangeLayer: {
            for (ui32 i = 0; i < editEvent.editCount; ++i) {
                TileContainerEditLayerEventData& edit = editEvent.changeLayerArray[i];
                const TileID prevId = edit.prevId;
                if (prevId != TILE_ID_NONE) {
                    const TileDef& prevTileData = TileRepository::get().getLoadedOrUnloadedAsset(edit.prevId);
                    if (prevTileData.shape == TileShape::MODEL) {
                        editEvents.removeEvents.emplace_back(edit.tileIndex);
                    }
                }
                const TileID newId = edit.newId;
                assert(newId != prevId);
                if (newId != TILE_ID_NONE) {
                    const TileDef& tileData = TileRepository::get().getLoadedOrUnloadedAsset(newId);
                    if (tileData.shape == TileShape::MODEL) {
                        editEvents.addEvents.emplace_back(ModelAddEvent{ edit.worldPosition, edit.tileIndex, tileData.modelId });
                    }
                }
            }
            break;
        }
        case TileContainerEditEventType::ChangeZPos:
            break;
        case TileContainerEditEventType::ChangeOrientation:
            break;
        default:
            assert(false && "Unhandled model edit event in InstancedStaticModelRenderer");
            break;
    }
    static_assert(e_cast(TileContainerEditEventType::TYPES) == 5, "Update handler");

    if (editEvents.removeEvents.size() || editEvents.addEvents.size()) {

        ModelEditEvents* editPtr = new ModelEditEvents(std::move(editEvents));

        RenderThreadTasks::getInstance().addGenericTask([](RenderContext& context, void* vEditsPtr) {
            ModelEditEvents* editPtr = static_cast<ModelEditEvents*>(vEditsPtr);
            TileContainerID containerId = editPtr->containerId;
            InstancedStaticModelManager* manager = editPtr->manager;
            for (auto&& index : editPtr->removeEvents) {
                manager->removeInstanceAtPosition(containerId, index);
            }
            for (auto&& addEvent : editPtr->addEvents) {
                manager->addInstanceAtPosition(containerId, addEvent.tileIndex, addEvent.modelId, addEvent.worldPosition, TileMeshBuilderMethods::getModelRotationAtPosition(addEvent.worldPosition));
            }
            delete editPtr;
        }, editPtr);
    }
}

void InstancedStaticModelManager::onTileDamagedEvent(const TileContainerEvent& evnt) {
    const TileDamagedEvent& damageEvent = std::get<TileDamagedEvent>(evnt.varEvent);
    // TODO: Instead of handling onTileDamagedEvent here, we should have a WorldVFXContext or something
    // which calls into the appropriate functions, this would ge replaced with onModelDamaged or something

    if (damageEvent.wasDestroyed) {
        return;
    }

    const TileDef& tileData = TileRepository::get().getLoadedOrUnloadedAsset(damageEvent.tileId);
    if (tileData.shape != TileShape::MODEL) {
        return;
    }

    if (evnt.container->isPendingDestroy()) {
        return;
    }

    struct TaskData {
        InstancedStaticModelManager* modelManager;
        TileContainer* container;
        const TileDef& tileData;
        TileDamagedEvent damageEvent;
    };

    TaskData* taskData = new TaskData{ .modelManager = this, .container = evnt.container, .tileData = tileData, .damageEvent = damageEvent };

    evnt.container->incRef();

    RenderThreadTasks::getInstance().addGenericTask([](RenderContext& context, void* vTaskDataPtr) {
        const TaskData* taskData = static_cast<const TaskData*>(vTaskDataPtr);
        TileDamagedEvent evnt = taskData->damageEvent;

        f32v2 hitNormal = evnt.impactNormal;
        hitNormal = MathUtil::rotateVector2D(hitNormal, 90.0f);

        const LiteTileHandle handle(taskData->container->getId(), evnt.tileIndex);
        taskData->modelManager->playAnimationOnInstanceAtPosition(handle, StaticModelAnimationTypes::HitWiggle, hitNormal);

        taskData->container->decRef();
        delete taskData;
    }, taskData);
}

void InstancedStaticModelManager::updatePendingModelDefs() {
    PROFILE_FUNCTION();
    ModelRepository& modelRepo = ModelRepository::get();
    for (auto&& it = mPendingInstances.begin(); it != mPendingInstances.end();) {
        if (const ModelDef* def = modelRepo.tryGetLoadedAsset(it->first)) {
            // Update data to point at now loaded mesh
            for (int m = 0; m < def->getNumMeshes(); ++m) {
                const Mesh& mesh = def->getMesh(m);
                const int renderPassIndex = e_cast(mesh.getRenderPass());
                StaticMeshInstanceData& instanceData = mModelsToInstances[renderPassIndex][def->getID()];
                instanceData.mMesh = &mesh;
            }
            // Add all instances
            for (PendingModelInstance& pendingInstance : it->second) {
                addInstanceAtPositionInternal(*def, pendingInstance.containerId, pendingInstance.tileIndex, pendingInstance.transform);
            }
            it = mPendingInstances.erase(it);
        }
        else {
            ++it;
        }
    }
}

void InstancedStaticModelManager::addInstanceAtPositionInternal(const ModelDef& modelDef, TileContainerID containerId, TileIndex tileIndex, const f32m4& transform) {
    for (int m = 0; m < modelDef.getNumMeshes(); ++m) {
        const Mesh& mesh = modelDef.getMesh(m);
        const int renderPassIndex = e_cast(mesh.getRenderPass());
        StaticMeshInstanceData& instanceData = mModelsToInstances[renderPassIndex][modelDef.getID()];

        const size_t instanceIndex = instanceData.mInstanceTransforms.size();
        if (instanceIndex < instanceData.mFirstDirtyInstance) {
            instanceData.mFirstDirtyInstance = instanceIndex;
        }
        // Store per tile references
        instanceData.mInstanceTransforms.emplace_back(transform);
        instanceData.mInstanceOwners.emplace_back(ModelInstanceOwner{ containerId, tileIndex });
        TileModelPositionKey positionKey{ tileIndex };

        SpatialInstanceDataMap& tileContainerModels = mTileContainerModels[renderPassIndex][containerId];
        assert(tileContainerModels.find(positionKey) == tileContainerModels.end());
        tileContainerModels[positionKey] = { modelDef.getID(), (ui32)instanceIndex };
    }
}

void InstancedStaticModelManager::updateAnimatedModels(f32 elapsedSec)
{
    std::vector<LiteTileHandle> animsToErase;

    for (auto&& it = mAnimatedInstances.begin(); it != mAnimatedInstances.end(); ++it) {

        StaticMeshAnimation& animation = it->second;
        const f32 animDuration = STATIC_MODEL_ANIM_DURATIONS_SEC[e_cast(animation.animType)];
        animation.currentTimeSec += elapsedSec;
        if (animation.currentTimeSec >= animDuration) {
            animsToErase.emplace_back(it->first);
            // TODO: Restore revious transform on GPU
        }
        else {

            TileModelInstance* instances[e_count(MaterialRenderPassType)];
            if (!getInstancesAtPosition(it->first, instances)) {
                return;
            }
            for (int rp = 0; rp < e_count(MaterialRenderPassType); ++rp) {
                TileModelInstance* instance = instances[rp];
                if (!instance) {
                    continue;
                }
                StaticMeshInstanceData& instanceData = mModelsToInstances[rp][instance->mModelID];
                const f32m4& baseTransform = instanceData.mInstanceTransforms[instance->mInstanceIndex];


                f32m4 newTransform;
                switch (animation.animType) {
                    case StaticModelAnimationTypes::HitWiggle: {
                        // https://www.wolframalpha.com/input?i=sin%28x%29+*+pow%28%288+*+PI+-+x%29+%2F+%288+*+pi%29%2C+2.0%29+from+0+to+8+*+pi
                        const f32 animAlpha = animation.currentTimeSec / animDuration;
                        constexpr f32 AMPLITUDE = 0.035f;
                        constexpr f32 PERIOD = 8.0f * M_PIF;
                        const f32 x = animAlpha * PERIOD;
                        const f32 rotationVal = sin(x) * powf((PERIOD - x) / PERIOD, 2.0f);

                        // Get axis of rotation relative to already rotated model
                        f32v3 rotateAxis = glm::inverse(baseTransform) * f32v4(animation.direction.x, animation.direction.y, 0.0f, 0.0f);
                        const f32m4 rotationMatrix = glm::rotate(f32m4(1.0f), rotationVal * AMPLITUDE, f32v3(rotateAxis.x, rotateAxis.y, rotateAxis.z));
                        newTransform = baseTransform * rotationMatrix;
                        break;
                    }
                    default:
                        assert(false);
                }
                static_assert(e_count(StaticModelAnimationTypes) == 1);

                // Override transform on gpu
                // TODO: MapUnmap will be faster maybe
                glNamedBufferSubData(
                    instanceData.mTransformsVbo,
                    instance->mInstanceIndex * sizeof(f32m4),
                    sizeof(f32m4),
                    &newTransform[0][0]
                );
            }
        }
    }

    for (auto&& h : animsToErase) {
        mAnimatedInstances.erase(h);
    }
   
}

void InstancedStaticModelManager::removeTileModelInstanceInternal(int renderPassIndex, TileModelInstance& instance)
{
    auto&& it = mModelsToInstances[renderPassIndex].find(instance.mModelID);
    assert(it != mModelsToInstances[renderPassIndex].end());
    StaticMeshInstanceData& instanceData = it->second;
    const ui32 instanceIndex = instance.mInstanceIndex;
    if (instanceIndex < instanceData.mFirstDirtyInstance) {
        instanceData.mFirstDirtyInstance = instanceIndex;
    }

    // Tell back owner about new position by grabbing transform position to look up
    ModelInstanceOwner backOwner = instanceData.mInstanceOwners.back();
    auto&& it2 = mTileContainerModels[renderPassIndex].find(backOwner.containerId);
    assert(it2 != mTileContainerModels[renderPassIndex].end());
    SpatialInstanceDataMap& backTileContainerModels = it2->second;
    TileModelPositionKey key{ backOwner.tileIndex };
    auto&& backRef = backTileContainerModels.find(key);
    assert(backRef != backTileContainerModels.end());
    backRef->second.mInstanceIndex = instanceIndex;

    // Replace this instance with back instance
    instanceData.mInstanceTransforms[instanceIndex] = std::move(instanceData.mInstanceTransforms.back());
    instanceData.mInstanceTransforms.pop_back();
    instanceData.mInstanceOwners[instanceIndex] = backOwner;
    instanceData.mInstanceOwners.pop_back();

    // If we are empty now, remove from the model map
    if (instanceData.mInstanceTransforms.empty()) {
        mModelsToInstances[renderPassIndex].erase(it);
    }
}

void InstancedStaticModelManager::decrefModelDef(ModelID modelId, int decCount) {
    assert(decCount > 0);
    auto&& mdrit = mModelDefRefs.find(modelId);
    assert(mdrit != mModelDefRefs.end());
    assert(mdrit->second.refCount >= decCount);
    mdrit->second.refCount -= decCount;
    if (mdrit->second.refCount == 0) {
        mModelDefRefs.erase(mdrit);
    }
}
