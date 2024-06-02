#include "stdafx.h"
#include "InstancedStaticModelManager.h"

#include "resources/ResourceManager.h"
#include "resources/ModelRepository.h"
#include "resources/TileRepository.h"
#include "tile/TileContainer.h"

#include "rendering/RenderContext.h"
#include "rendering/MaterialShaderRepository.h"
#include "rendering/model/InstancedStaticModelGatherer.h"
#include "rendering/model/ModelUtil.h"
#include "rendering/mesh/mesher/builder/ModelMeshBuilder.h"
#include "rendering/RenderThreadTasks.h"
#include "rendering/mesh/mesher/builder/TileMeshBuilderMethods.h"
#include "rendering/model/ModelBillboardLodManager.h"

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
    mBillboardLodManager = std::make_unique<ModelBillboardLodManager>(RenderContext::getInstance().getModelBillboardLodBuilder().getBillboardTextures());
}

InstancedStaticModelManager::~InstancedStaticModelManager() {
    for (auto& it : mModelBatches) {
        GL.glDeleteBuffers(1, &it.second.mTransformsVbo);
        GL.glDeleteBuffers(1, &it.second.mVariantsVbo);
    }
}

void InstancedStaticModelManager::frameUpdate(const Camera3D& camera, f32 elapsedSec) {
    ASSERT_RENDER_THREAD();
    if (sDebugOptions.mHideModels)
        return;

    PROFILE_FUNCTION();

    updatePendingLooseModelInstances();

    // Build GPU data and cull
    for (auto& it : mModelBatches) {
        StaticModelBatchData& batchData = it.second;
        if (!batchData.mIsLoaded) [[unlikely]] {
            auto&& sit = mModelDefRefs.find(it.first);
            assert(sit != mModelDefRefs.end());
            AssetHandlePtr<ModelDef>& modelDefHandle = sit->second.handle;
            if (const ModelDef* def = modelDefHandle->tryGetLoadedAsset()) {
                batchData.mMeshCount = def->getNumMeshes();
                for (int m = 0; m < batchData.mMeshCount; ++m) {
                    const Mesh& mesh = def->getMesh(m);
                    batchData.mMesh[m] = &mesh;
                    batchData.mMeshCastsShadow[m] = mesh.castsShadow();
                }
                batchData.mIsLoaded = true;
            }
            else {
                continue;
            }
        }
        // TODO: Move this to onRemove
        if (!batchData.mInstanceTransforms.size()) {
            for (int i = 0; i < batchData.mMeshCount; ++i) {
                batchData.mDrawCommands[i].reset();
                batchData.mDrawCommandsShadows[i].reset();
            }
            if (batchData.mTransformsVbo) {
                GL.glDeleteBuffers(1, &batchData.mTransformsVbo);
                GL.glDeleteBuffers(1, &batchData.mVariantsVbo);
                batchData.mTransformsVbo = 0;
                batchData.mVariantsVbo = 0;
            }
            continue;
        }
        assert(batchData.mMesh);

        ModelID modelId = it.first;
        const ModelLodParams& lodParams = ModelRepository::get().getLodParams(modelId);
        MeshLODDrawInfo drawInfos[e_count(MaterialRenderPassType)][4];
        for (int m = 0; m < batchData.mMeshCount; ++m) {
            for (int i = 0; i < 4; ++i) {
                drawInfos[m][i] = batchData.mMesh[m]->mGpuData.mLODData.getDrawInfoForLOD(MeshLODLevel(i));
            }
        }
        if (batchData.mFirstDirtyInstance != UINT32_MAX) {
            PROFILE_SCOPE("Rebuild Indirect Buffer");
            const size_t workGroupRoundedSize = roundToWorkGroupSize(batchData.mInstanceTransforms.size());
            // Rebuild command buffer
            {
                PROFILE_SCOPE("Indirect Buffer");
                for (int m = 0; m < batchData.mMeshCount; ++m) {
                    batchData.mDrawCommands[m] = std::make_unique<GLDrawCommandBuffer>(workGroupRoundedSize);
                    if (batchData.mMeshCastsShadow[m]) {
                        batchData.mDrawCommandsShadows[m] = std::make_unique<GLDrawCommandBuffer>(workGroupRoundedSize);
                    }

                }
                // TODO: Only if the mesh is a shadow caster!
            }

            // Allocate VBO
            {
                PROFILE_SCOPE("VBO");
                // GPU buffer is larger to accommodate the work group size, or we get corruption
                const GLsizei gpuBufferSizeBytes = sizeof(f32m4) * workGroupRoundedSize;
                const GLsizei cpuBufferSizeBytes = sizeof(f32m4) * batchData.mInstanceTransforms.size();
                assert(batchData.mInstanceVariants.size() == batchData.mInstanceTransforms.size());
                if (batchData.mTransformsVbo == 0) {
                    GL.glCreateBuffers(1, &batchData.mTransformsVbo);
                    GL.glCreateBuffers(1, &batchData.mVariantsVbo);
                    for (int m = 0; m < batchData.mMeshCount; ++m) {
                        batchData.mMesh[m]->bindStaticModelAttribs();
                    }
                    GL.glNamedBufferStorage(batchData.mTransformsVbo, gpuBufferSizeBytes, nullptr, GL_DYNAMIC_STORAGE_BIT);
                    GL.glNamedBufferSubData(batchData.mTransformsVbo, 0, cpuBufferSizeBytes, batchData.mInstanceTransforms.data());
                    GL.glNamedBufferStorage(batchData.mVariantsVbo, sizeof(ui8) * workGroupRoundedSize, nullptr, GL_DYNAMIC_STORAGE_BIT);
                    GL.glNamedBufferSubData(batchData.mVariantsVbo, 0, sizeof(ui8) * batchData.mInstanceVariants.size(), batchData.mInstanceVariants.data());
                    batchData.mTransformsVboSizeBytes = gpuBufferSizeBytes;
                }
                else if (gpuBufferSizeBytes > batchData.mTransformsVboSizeBytes) {
                    //LOG_INFO("GROW {} {}", cpuBufferSizeBytes, gpuBufferSizeBytes);
                    // Grow to new size
                    GL.glDeleteBuffers(1, &batchData.mTransformsVbo);
                    GL.glDeleteBuffers(1, &batchData.mVariantsVbo);
                    GL.glCreateBuffers(1, &batchData.mTransformsVbo);
                    GL.glCreateBuffers(1, &batchData.mVariantsVbo);
                    GL.glNamedBufferStorage(batchData.mTransformsVbo, gpuBufferSizeBytes, nullptr, GL_DYNAMIC_STORAGE_BIT);
                    GL.glNamedBufferSubData(batchData.mTransformsVbo, 0, cpuBufferSizeBytes, batchData.mInstanceTransforms.data());
                    GL.glNamedBufferStorage(batchData.mVariantsVbo, sizeof(ui8) * workGroupRoundedSize, nullptr, GL_DYNAMIC_STORAGE_BIT);
                    GL.glNamedBufferSubData(batchData.mVariantsVbo, 0, sizeof(ui8) * batchData.mInstanceVariants.size(), batchData.mInstanceVariants.data());
                    batchData.mTransformsVboSizeBytes = gpuBufferSizeBytes;
                }
                else {
                    //LOG_INFO("SHRINK {} {}  {} {}", instanceData.mFirstDirtyInstance, instanceData.mInstanceTransforms.size(), cpuBufferSizeBytes, gpuBufferSizeBytes);
                    // Only upload data after the first dirty instance, which should amortize things a bit
                    glNamedBufferSubData(
                        batchData.mTransformsVbo,
                        batchData.mFirstDirtyInstance * sizeof(f32m4),
                        cpuBufferSizeBytes - batchData.mFirstDirtyInstance * sizeof(f32m4),
                        batchData.mInstanceTransforms.data() + batchData.mFirstDirtyInstance
                    );
                    glNamedBufferSubData(
                        batchData.mVariantsVbo,
                        batchData.mFirstDirtyInstance * sizeof(ui8),
                        (sizeof(ui8) * batchData.mInstanceVariants.size()) - batchData.mFirstDirtyInstance * sizeof(ui8),
                        batchData.mInstanceVariants.data() + batchData.mFirstDirtyInstance
                    );
                }
            }

            batchData.mFirstDirtyInstance = UINT32_MAX;
        }

        const size_t drawCommandsCapacity = batchData.mDrawCommands[0]->getCapacity();

        for (int m = 0; m < batchData.mMeshCount; ++m) {
            batchData.mDrawCommands[m]->frameBegin();
            // TODO: Only if the mesh is a shadow caster!
            if (batchData.mDrawCommandsShadows[m]) {
                batchData.mDrawCommandsShadows[m]->frameBegin();
            }
        }
        // TODO: Only if the mesh is a shadow caster!

        if (sDebugOptions.mDisableGPUCulling == false) {
            PROFILE_SCOPE("GPU Culling");
            // GPU Culling
            panic("GPU Culling is defunct");
            //GpuCullUniformData uniformData;
            //const f32v3& camPos = camera.getPosition();
            //uniformData.cameraPos = f32v4(camPos.x, camPos.y, camPos.z, 1.0f);
            //uniformData.numShapesToCull = drawCommandsCapacity;
            //if (sDebugOptions.mDisableLOD) {
            //    uniformData.lodDistancesSQ[0] = FLT_MAX;
            //}
            //else {
            //    for (int i = 0; i < 4; ++i) {
            //        uniformData.lodDistancesSQ[i] = lodParams.lodDistancesSQ[i];
            //    }
            //}
            //for (int i = 0; i < 4; ++i) {
            //    uniformData.frustumPlanes[i] = camera.getFrustum().getPlane(i).vec4Data;
            //    uniformData.lodDrawInfos[i] = mesh.mGpuData.mLODData.getDrawInfoForLOD(MeshLODLevel(i));
            //}

            //mGpuCullingUniformBuffer.updateSubData(0, sizeof(GpuCullUniformData), &uniformData);
            ////*instanceData.mNumVisibleMeshesBufferPtr = 0; // Compact indirect buffer is actually slower due to atomic operation and cpu-gpu sync

            //if (const MaterialShaderDef* def = mCullingComputeShader->tryGetLoadedAsset()) {
            //    def->useCompute();
            //    glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
            //    GL.glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, instanceData.mTransformsVbo);
            //    GL.glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, inDrawCommands.getHandle());
            //    GL.glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, inDrawCommandsShadows.getHandle());
            //    GL.glBindBufferBase(GL_UNIFORM_BUFFER, 5, mGpuCullingUniformBuffer.getHandle());
            //    //glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, instanceData.mNumVisibleMeshesBuffer.getHandle());
            //    //glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, outDrawCommands.getHandle()); // Compact indirect buffer is actually slower due to atomic operation and cpu-gpu sync
            //    if (drawCommandsCapacity % WORK_GROUP_SIZE == 0) {
            //        glDispatchCompute((GLuint)drawCommandsCapacity / WORK_GROUP_SIZE, 1, 1);
            //    }
            //    else {
            //        glDispatchCompute(1 + (GLuint)drawCommandsCapacity / WORK_GROUP_SIZE, 1, 1);
            //    }
            //    glMemoryBarrier(GL_COMMAND_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT); // GL_ATOMIC_COUNTER_BARRIER_BIT

            //    inDrawCommands.setNumActiveCommands(drawCommandsCapacity);
            //    inDrawCommandsShadows.setNumActiveCommands(drawCommandsCapacity);
            //}
        }
        else {
            assert(batchData.mInstanceTransforms.size() <= drawCommandsCapacity);
            PROFILE_SCOPE("CPU Culling");
            // CPU Culling
            // TODO: Should the renderer handle this??
            int activeCount[e_count(MaterialRenderPassType)] = {};
            int shadowCount[e_count(MaterialRenderPassType)] = {};

            constexpr auto setCommand = [](DrawElementsIndirectCommand& cmd, GLuint transformIndex, const MeshLODDrawInfo& drawInfo) {
                cmd.instanceCount_ = 1;
                cmd.baseInstance_ = transformIndex;
                cmd.baseVertex_ = 0;
                cmd.count_ = drawInfo.indexCount;
                cmd.firstIndex_ = drawInfo.startIndex;
            };

            for (size_t i = 0; i < batchData.mInstanceTransforms.size(); ++i) {
                const f32m4& transform = batchData.mInstanceTransforms[i];
                // Columns are first
                const f32v3& pos = reinterpret_cast<const f32v3&>(transform[3]);
                // TODO: Real bounding sphere
                if (camera.sphereIsVisible(pos, lodParams.boundingSphereRadius)) {
                    MeshLODDrawInfo drawInfo;
                    const f32 distance2 = glm::length2(pos - camera.getPosition());

                    // TODO: Remove
                    if (sDebugOptions.mDisableLOD) [[unlikely]] {
                        for (int m = 0; m < batchData.mMeshCount; ++m) {
                            setCommand(batchData.mDrawCommands[m]->getDrawCommands().data()[activeCount[m]++], (GLuint)i, drawInfos[m][1]);
                            if (batchData.mDrawCommandsShadows[m]) [[likely]] {
                                setCommand(batchData.mDrawCommandsShadows[m]->getDrawCommands().data()[shadowCount[m]++], (GLuint)i, drawInfos[m][1]);
                            }
                        }
                    }

                    if (distance2 >= lodParams.lodDistancesSQ[3]) {
                        // TODO: Billboard
                        continue;
                    }
                    else if (distance2 >= lodParams.lodDistancesSQ[2]) {
                        for (int m = 0; m < batchData.mMeshCount; ++m) {
                            setCommand(batchData.mDrawCommands[m]->getDrawCommands().data()[activeCount[m]++], (GLuint)i, drawInfos[m][3]);
                           
                        }
                        if (lodParams.shadowLodDetail > ShadowModelDetail::High) {
                            for (int m = 0; m < batchData.mMeshCount; ++m) {
                                if (batchData.mDrawCommandsShadows[m]) [[likely]] {
                                    setCommand(batchData.mDrawCommandsShadows[m]->getDrawCommands().data()[shadowCount[m]++], (GLuint)i, drawInfos[m][3]);
                                }
                            }
                        }
                    }
                    else if (distance2 >= lodParams.lodDistancesSQ[1]) {
                        for (int m = 0; m < batchData.mMeshCount; ++m) {
                            setCommand(batchData.mDrawCommands[m]->getDrawCommands().data()[activeCount[m]++], (GLuint)i, drawInfos[m][2]);
                        }
                        if (lodParams.shadowLodDetail > ShadowModelDetail::Medium) {
                            for (int m = 0; m < batchData.mMeshCount; ++m) {
                                if (batchData.mDrawCommandsShadows[m]) [[likely]] {
                                    setCommand(batchData.mDrawCommandsShadows[m]->getDrawCommands().data()[shadowCount[m]++], (GLuint)i, drawInfos[m][3]);
                                }
                            }
                        }
                    }
                    else if (distance2 >= lodParams.lodDistancesSQ[0]) {
                        for (int m = 0; m < batchData.mMeshCount; ++m) {
                            setCommand(batchData.mDrawCommands[m]->getDrawCommands().data()[activeCount[m]++], (GLuint)i, drawInfos[m][1]);
                        }
                        if (lodParams.shadowLodDetail > ShadowModelDetail::Low) {
                            for (int m = 0; m < batchData.mMeshCount; ++m) {
                                if (batchData.mDrawCommandsShadows[m]) [[likely]] {
                                    setCommand(batchData.mDrawCommandsShadows[m]->getDrawCommands().data()[shadowCount[m]++], (GLuint)i, drawInfos[m][2]);
                                }
                            }
                        }
                    }
                    else {
                        for (int m = 0; m < batchData.mMeshCount; ++m) {
                            setCommand(batchData.mDrawCommands[m]->getDrawCommands().data()[activeCount[m]++], (GLuint)i, drawInfos[m][0]);
                        }
                        if (lodParams.shadowLodDetail > ShadowModelDetail::None) {
                            for (int m = 0; m < batchData.mMeshCount; ++m) {
                                if (batchData.mDrawCommandsShadows[m]) [[likely]] {
                                    setCommand(batchData.mDrawCommandsShadows[m]->getDrawCommands().data()[shadowCount[m]++], (GLuint)i, drawInfos[m][1]);
                                }
                            }
                        }
                    }
                }
            }
            for (int m = 0; m < batchData.mMeshCount; ++m) {
                batchData.mDrawCommands[m]->setNumActiveCommands(activeCount[m]);
                batchData.mDrawCommands[m]->uploadDrawCommands();
                if (batchData.mDrawCommandsShadows[m]) {
                    batchData.mDrawCommandsShadows[m]->setNumActiveCommands(shadowCount[m]);
                    batchData.mDrawCommandsShadows[m]->uploadDrawCommands();
                }
            }
        }

        // Compact indirect buffer is actually slower due to atomic operation and cpu-gpu sync
        // Sync start of draw
        /* if (inDrawCommands.mDrawCommands.size()) {
            glMemoryBarrier(GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT);
            instanceData.mFenceSync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
        }*/

    }

    updateAnimatedModels(elapsedSec);
}

void InstancedStaticModelManager::addTileInstanceAtPosition(TileContainerID containerId, TileIndex tileIndex, ModelID modelId, f32v3 position, f32 rotation, ui8 variantIndex) {
    ASSERT_RENDER_THREAD();

    increfModelDef(modelId, 1);
    
    const ModelDef* modelDefPtr = ModelRepository::get().tryGetLoadedAsset(modelId);
    assert(modelDefPtr);

    addTileInstanceInternal(*modelDefPtr, containerId, tileIndex, ModelUtil::computeTransformMatrixForModel(position, rotation), variantIndex);
}

void InstancedStaticModelManager::removeTileInstanceAtPosition(TileContainerID containerId, TileIndex tileIndex) {
    ASSERT_RENDER_THREAD();

    auto&& it = mTileContainerTrackedModels.find(containerId);
    if (it != mTileContainerTrackedModels.end()) {
        SpatialInstanceDataMap& spatialMap = it->second;
        auto&& spit = spatialMap.find(tileIndex);

        if (spit != spatialMap.end()) {
            removeTileInstanceInternal(spit->second);
            spatialMap.erase(spit);
            if (spatialMap.empty()) {
                mTileContainerTrackedModels.erase(it);
            }
        }
    }
}

TileModelInstance* InstancedStaticModelManager::getTileInstanceAtPosition(LiteTileHandle tileHandle) {
    ASSERT_RENDER_THREAD();
    auto&& it = mTileContainerTrackedModels.find(tileHandle.containerId);
    if (it != mTileContainerTrackedModels.end()) {
        SpatialInstanceDataMap& spatialMap = it->second;
        auto&& spit = spatialMap.find(tileHandle.index);

        if (spit != spatialMap.end()) {
            return &spit->second;
        }
    }
    return nullptr;
}

bool InstancedStaticModelManager::hasTileInstanceAtPosition(LiteTileHandle tileHandle) {
    auto&& it = mTileContainerTrackedModels.find(tileHandle.containerId);
    if (it != mTileContainerTrackedModels.end()) {
        SpatialInstanceDataMap& spatialMap = it->second;
        auto&& spit = spatialMap.find(tileHandle.index);

        if (spit != spatialMap.end()) {
            return true;
        }
    }
    return false;
}

void InstancedStaticModelManager::addTileInstancesFromGatherer(InstancedStaticModelGatherer& gatherer)
{
    ASSERT_RENDER_THREAD();
    PROFILE_FUNCTION();
    if (gatherer.mInstances.empty()) {
        return;
    }
    // Remove all instances before we add new ones
    removeTileInstancesFromContainer(gatherer.mContainerID);

    for (auto&& it : gatherer.mInstances) {

        ModelID modelId = it.first;
        const std::vector<StaticModelInstance>& sourceInstances = it.second;

        increfModelDef(modelId, sourceInstances.size());

        SpatialInstanceDataMap& tileContainerModels = mTileContainerTrackedModels[gatherer.mContainerID];
        StaticModelBatchData& batchData = mModelBatches[modelId];

        const size_t startIndex = batchData.mInstanceTransforms.size();
        // Track where our buffer is dirty
        if (startIndex < batchData.mFirstDirtyInstance) {
            batchData.mFirstDirtyInstance = startIndex;
        }
        batchData.mInstanceTransforms.resize(startIndex + sourceInstances.size());
        batchData.mInstanceVariants.resize(startIndex + sourceInstances.size());
        batchData.mInstanceSources.resize(batchData.mInstanceTransforms.size());
        // Store per tile references
        for (size_t i = 0; i < sourceInstances.size(); ++i) {
            size_t instanceIndex = startIndex + i;
            const StaticModelInstance& modelInstance = sourceInstances[i];
            batchData.mInstanceTransforms[instanceIndex] = modelInstance.matrix;
            batchData.mInstanceVariants[instanceIndex] = modelInstance.variantIndex;
            batchData.mInstanceSources[instanceIndex] = ModelInstanceContainerOwner{ gatherer.mContainerID, modelInstance.tileIndex };
            assert(tileContainerModels.find(modelInstance.tileIndex) == tileContainerModels.end());
            tileContainerModels[modelInstance.tileIndex] = { it.first, (ui32)instanceIndex };
        }
    }
}

void InstancedStaticModelManager::removeTileInstancesFromContainer(TileContainerID containerId)
{
    // TODO: There is a race condition if the tile container is being meshed. Make sure we only destroy tile containers once they are done
    // being meshed?
    ASSERT_RENDER_THREAD();

    auto&& it = mTileContainerTrackedModels.find(containerId);
    if (it != mTileContainerTrackedModels.end()) {
        SpatialInstanceDataMap& tileContainerModels = it->second;
        for (auto& it2 : tileContainerModels) {
            removeTileInstanceInternal(it2.second);
        }
        mTileContainerTrackedModels.erase(it);
    }
}

ui32 InstancedStaticModelManager::getNumModels() const {
    ASSERT_RENDER_THREAD();
    ui32 numModels = 0;
    for (auto& it : mModelBatches) {
        numModels += it.second.mInstanceTransforms.size();
    }
    return numModels;
}

void InstancedStaticModelManager::playAnimationOnInstanceAtPosition(LiteTileHandle targetTile, StaticModelAnimationTypes animType, f32v2 direction) {
    // Ensure tile exists as static model
    if (!hasTileInstanceAtPosition(targetTile)) {
        return;
    }

    // Overwrite existing animation if any
    StaticMeshAnimation& anim = mAnimatedTileInstances[targetTile];
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
                manager->removeTileInstanceAtPosition(containerId, index);
            }
            for (auto&& addEvent : editPtr->addEvents) {
                manager->addTileInstanceAtPosition(containerId, addEvent.tileIndex, addEvent.modelId, addEvent.worldPosition, TileMeshBuilderMethods::getModelRotationAtPosition(addEvent.worldPosition), 0 /*TODO: Variant*/);
            }
            delete editPtr;
        }, editPtr);
    }
}

void InstancedStaticModelManager::onTileDamagedEvent(const TileContainerEvent& evnt) {
    const TileDamagedEvent& damageEvent = std::get<TileDamagedEvent>(evnt.varEvent);
    // TODO: Instead of handling onTileDamagedEvent here, we should have a WorldVFXContext or something
    // which calls into the appropriate functions, this would get replaced with onModelDamaged or something

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

StaticModelInstanceID InstancedStaticModelManager::addLooseModelInstance(ModelID modelId, const glm::quat& orient, f32v3 position, ui8 variantIndex)
{
    StaticModelInstanceID id;
    {
        std::lock_guard lock(mLooseInstanceIDMutex);
        id = mNextLooseInstanceIDs[modelId]++;
    }

    PendingLooseModelInstance pendingInstance{
        .orient = orient,
        .position = position,
        .modelId = modelId,
        .instanceId = id,
        .variantIndex = variantIndex,
        .isRemove = false
    };

    mPendingLooseModelInstances.enqueue(pendingInstance);

    return id;
}

void InstancedStaticModelManager::removeLooseModelInstance(ModelID modelId, StaticModelInstanceID instanceId) {
    PendingLooseModelInstance pendingInstance{
        .modelId = modelId,
        .instanceId = instanceId,
        .isRemove = true
    };

    mPendingLooseModelInstances.enqueue(pendingInstance);
}

void InstancedStaticModelManager::updatePendingLooseModelInstances() {
    PROFILE_FUNCTION();
    constexpr i32 MAX_DEQUEUE = 1024;
    PendingLooseModelInstance instances[MAX_DEQUEUE];
    if (size_t count = mPendingLooseModelInstances.try_dequeue_bulk(instances, MAX_DEQUEUE)) {
        for (size_t i = 0; i < count; ++i) {
            PendingLooseModelInstance& instance = instances[i];
            if (instance.isRemove) {
                removeLooseInstanceInternal(instance.modelId, instance.instanceId);
            }
            else {
                // TODO: Construct transform in place so no copy?
                const f32m4 transform = MathUtil::createTransformMatrix(instance.position, instance.orient);
                addLooseInstanceInternal(instance.modelId, instance.instanceId, transform, instance.variantIndex);
            }
        }
    }
}

void InstancedStaticModelManager::removeModelInstanceInternal(StaticModelBatchData& batchData, ui32 instanceIndex, ModelID modelId) {

    if (instanceIndex < batchData.mFirstDirtyInstance) {
        batchData.mFirstDirtyInstance = instanceIndex;
    }

    ModelInstanceOwnerVariant backSource = batchData.mInstanceSources.back();
    // Tell back source about new position by grabbing transform position to look up
    // as we will be swapping and popping
    if (std::holds_alternative<ModelInstanceContainerOwner>(backSource)) {
        // Tile container model
        ModelInstanceContainerOwner& owner = std::get<ModelInstanceContainerOwner>(backSource);
        auto&& it2 = mTileContainerTrackedModels.find(owner.containerId);
        assert(it2 != mTileContainerTrackedModels.end());
        SpatialInstanceDataMap& backTileContainerModels = it2->second;
        auto&& backRef = backTileContainerModels.find(owner.tileIndex);
        assert(backRef != backTileContainerModels.end());
        backRef->second.mInstanceIndex = instanceIndex;
    }
    else {
        // Loose model
        auto&& it2 = mLooseStaticModelInstances.find(modelId);
        assert(it2 != mLooseStaticModelInstances.end());
        const StaticModelInstanceID backInstanceID = std::get<StaticModelInstanceID>(backSource);
        auto&& backRef = it2->second.find(backInstanceID);
        assert(backRef != it2->second.end());
        backRef->second = instanceIndex;
    }
    batchData.mInstanceSources[instanceIndex] = backSource;
    batchData.mInstanceSources.pop_back();


    // Replace this instance with back instance
    batchData.mInstanceTransforms[instanceIndex] = std::move(batchData.mInstanceTransforms.back());
    batchData.mInstanceTransforms.pop_back();
    batchData.mInstanceVariants[instanceIndex] = std::move(batchData.mInstanceVariants.back());
    batchData.mInstanceVariants.pop_back();
}

void InstancedStaticModelManager::addTileInstanceInternal(const ModelDef& modelDef, TileContainerID containerId, TileIndex tileIndex, const f32m4& transform, ui8 variantIndex) {

    StaticModelBatchData& batchData = mModelBatches[modelDef.getID()];

    const size_t instanceIndex = batchData.mInstanceTransforms.size();
    if (instanceIndex < batchData.mFirstDirtyInstance) {
        batchData.mFirstDirtyInstance = instanceIndex;
    }
    // Store per tile references
    batchData.mInstanceTransforms.emplace_back(transform);
    batchData.mInstanceVariants.emplace_back(variantIndex);
    batchData.mInstanceSources.emplace_back(ModelInstanceContainerOwner{ containerId, tileIndex });
    SpatialInstanceDataMap& tileContainerModels = mTileContainerTrackedModels[containerId];

    assert(tileContainerModels.find(tileIndex) == tileContainerModels.end());
    tileContainerModels[tileIndex] = { modelDef.getID(), (ui32)instanceIndex };
}

void InstancedStaticModelManager::removeTileInstanceInternal(TileModelInstance& instance) {
    auto&& it = mModelBatches.find(instance.mModelID);
    assert(it != mModelBatches.end());
    StaticModelBatchData& batchData = it->second;
    const ui32 instanceIndex = instance.mInstanceIndex;

    removeModelInstanceInternal(batchData, instanceIndex, instance.mModelID);

    // If we are empty now, remove from the model map
    if (batchData.mInstanceTransforms.empty()) {
        mModelBatches.erase(it);
    }
}


void InstancedStaticModelManager::addLooseInstanceInternal(ModelID modelId, StaticModelInstanceID instanceId, const f32m4& transform, ui8 variantIndex) {

    increfModelDef(modelId, 1);

    StaticModelBatchData& batchData = mModelBatches[modelId];

    const size_t instanceIndex = batchData.mInstanceTransforms.size();
    if (instanceIndex < batchData.mFirstDirtyInstance) {
        batchData.mFirstDirtyInstance = instanceIndex;
    }
    batchData.mInstanceTransforms.emplace_back(transform);
    batchData.mInstanceVariants.emplace_back(variantIndex);
    batchData.mInstanceSources.emplace_back(instanceId);

    // Store instance lookup
    auto&& mp = mLooseStaticModelInstances[modelId];
    mp.emplace(std::make_pair(instanceId, (ui32)instanceIndex));
}

void InstancedStaticModelManager::removeLooseInstanceInternal(ModelID modelId, StaticModelInstanceID instanceId) {
    auto&& it = mModelBatches.find(modelId);
    assert(it != mModelBatches.end());
    StaticModelBatchData& batchData = it->second;

    auto&& lit = mLooseStaticModelInstances.find(modelId);
    assert(lit != mLooseStaticModelInstances.end());
    auto&& instanceIt = lit->second.find(instanceId);
    assert(instanceIt != lit->second.end());
    const ui32 instanceIndex = instanceIt->second;

    ModelInstanceOwnerVariant backOwner = batchData.mInstanceSources.back();
    removeModelInstanceInternal(batchData, instanceIndex, modelId);

    // Remove our tracked instance
    lit->second.erase(instanceIt);

    // If we are empty now, remove from the model map
    if (batchData.mInstanceTransforms.empty()) {
        mModelBatches.erase(it);
    }
}

void InstancedStaticModelManager::updateAnimatedModels(f32 elapsedSec)
{
    std::vector<LiteTileHandle> animsToErase;

    for (auto&& it = mAnimatedTileInstances.begin(); it != mAnimatedTileInstances.end(); ++it) {

        StaticMeshAnimation& animation = it->second;
        const f32 animDuration = STATIC_MODEL_ANIM_DURATIONS_SEC[e_cast(animation.animType)];
        animation.currentTimeSec += elapsedSec;
        if (animation.currentTimeSec >= animDuration) {
            animsToErase.emplace_back(it->first);
            // TODO: Restore previous transform on GPU
        }
        else {

            TileModelInstance* instance = getTileInstanceAtPosition(it->first);
            if (!instance) {
                return;
            }
            StaticModelBatchData& instanceData = mModelBatches[instance->mModelID];
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
            // TODO: MapUnmap will be faster maybe?
            glNamedBufferSubData(
                instanceData.mTransformsVbo,
                instance->mInstanceIndex * sizeof(f32m4),
                sizeof(f32m4),
                &newTransform[0][0]
            );
        }
    }

    for (auto&& h : animsToErase) {
        mAnimatedTileInstances.erase(h);
    }
}

void InstancedStaticModelManager::increfModelDef(ModelID modelId, int incCount) {
    assert(incCount > 0);
    auto&& mdrit = mModelDefRefs.find(modelId);
    if (mdrit == mModelDefRefs.end()) {
        ModelDefRef newRef;
        newRef.handle = ModelRepository::get().getAssetHandle(modelId);
        newRef.refCount = incCount;
        mModelDefRefs.emplace(std::make_pair(modelId, std::move(newRef)));
    }
    else {
        mdrit->second.refCount += incCount;
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
