#include "stdafx.h"
#include "InstancedStaticModelManager.h"

#include "resources/ResourceManager.h"
#include "resources/ModelRepository.h"
#include "resources/TileRepository.h"
#include "tile/TileContainer.h"

#include "rendering/MaterialShaderManager.h"
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

// TODO: One less indirect? Store modelrepository?
inline int getRenderPassIndexForModel(ModelID modelId) {
    return e_cast(Services::ResourceManager::ref().getModelRepository().getModelDef(modelId).mRenderPass);
}

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
    const MaterialShaderManager& materialManager = Services::ResourceManager::ref().getMaterialShaderManager();
    mCullingComputeShader = materialManager.getComputeShader("culling_and_lod");
}

InstancedStaticModelManager::~InstancedStaticModelManager() {
    for (int ri = 0; ri < e_cast(MaterialRenderPassType::COUNT); ++ri) {
        for (auto& it : mModelsToInstances[ri]) {
            GL.glDeleteBuffers(1, &it.second.mTransformsVbo);
        }
    }
}

void InstancedStaticModelManager::frameUpdate(const Camera3D& camera)
{
    ASSERT_RENDER_THREAD();
    if (sDebugOptions.mHideModels)
        return;

    PROFILE_FUNCTION();

    for (int ri = 0; ri < e_cast(MaterialRenderPassType::COUNT); ++ri) {
        for (auto& it : mModelsToInstances[ri]) {
            StaticModelInstanceData& instanceData = it.second;
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

            ModelID modelId = it.first;
            const Model3D& model = Services::ResourceManager::ref().getModelRepository().getModelDef(modelId).mModel;
            const Mesh& mesh = *model.getMesh();
            MeshLODDrawInfo drawInfos[4];
            for (int i = 0; i < 4; ++i) {
                drawInfos[i] = mesh.mMainMesh.mLODData.getDrawInfoForLOD(MeshLODLevel(i));
            }

            if (instanceData.mFirstDirtyInstance != UINT32_MAX) {
                PROFILE_SCOPE("Rebuild Indirect Buffer");
                const size_t workGroupRoundedSize = roundToWorkGroupSize(instanceData.mInstanceTransforms.size());
                // Rebuild command buffer
                {
                    PROFILE_SCOPE("Indirect Buffer");
                    instanceData.mDrawCommands = std::make_unique<GLIndirectBuffer>(workGroupRoundedSize);
                    instanceData.mDrawCommandsShadows = std::make_unique<GLIndirectBuffer>(workGroupRoundedSize);
                    // This is now initialized on the gpu
                    //for (size_t i = 0; i < instanceData.mDrawCommands->mDrawCommands.size(); ++i) {
                    //    DrawElementsIndirectCommand& cmd = instanceData.mDrawCommands->mDrawCommands[i];
                    //    cmd.baseInstance_ = i;
                    //}
                    //instanceData.mDrawCommands->uploadIndirectBuffer();
                }

                // Allocate VBO
                {
                    PROFILE_SCOPE("VBO");
                    // GPU buffer is larger to accomidate the work group size, or we get corruption
                    const GLsizei gpuBufferSizeBytes = sizeof(f32m4) * workGroupRoundedSize;
                    const GLsizei cpuBufferSizeBytes = sizeof(f32m4) * instanceData.mInstanceTransforms.size();
                    constexpr GLuint BINDING_POINT = 2;
                    if (instanceData.mTransformsVbo == 0) {
                        GL.glCreateBuffers(1, &instanceData.mTransformsVbo);
                        glEnableVertexArrayAttrib(mesh.mMainMesh.mVao, 7);
                        glEnableVertexArrayAttrib(mesh.mMainMesh.mVao, 8);
                        glEnableVertexArrayAttrib(mesh.mMainMesh.mVao, 9);
                        glEnableVertexArrayAttrib(mesh.mMainMesh.mVao, 10);
                        glVertexArrayAttribFormat(mesh.mMainMesh.mVao, 7, 4, GL_FLOAT, GL_FALSE, 0);
                        glVertexArrayAttribFormat(mesh.mMainMesh.mVao, 8, 4, GL_FLOAT, GL_FALSE, sizeof(f32v4));
                        glVertexArrayAttribFormat(mesh.mMainMesh.mVao, 9, 4, GL_FLOAT, GL_FALSE, sizeof(f32v4) * 2.0f);
                        glVertexArrayAttribFormat(mesh.mMainMesh.mVao, 10, 4, GL_FLOAT, GL_FALSE, sizeof(f32v4) * 3.0f);
                        glVertexArrayAttribBinding(mesh.mMainMesh.mVao, 7, BINDING_POINT);
                        glVertexArrayAttribBinding(mesh.mMainMesh.mVao, 8, BINDING_POINT);
                        glVertexArrayAttribBinding(mesh.mMainMesh.mVao, 9, BINDING_POINT);
                        glVertexArrayAttribBinding(mesh.mMainMesh.mVao, 10, BINDING_POINT);
                        glVertexArrayBindingDivisor(mesh.mMainMesh.mVao, BINDING_POINT, 1);
                        GL.glNamedBufferStorage(instanceData.mTransformsVbo, gpuBufferSizeBytes, nullptr, GL_DYNAMIC_STORAGE_BIT);
                        GL.glNamedBufferSubData(instanceData.mTransformsVbo, 0, cpuBufferSizeBytes, instanceData.mInstanceTransforms.data());
                        GL.glVertexArrayVertexBuffer(mesh.mMainMesh.mVao, BINDING_POINT, instanceData.mTransformsVbo, 0, sizeof(f32m4));
                        instanceData.mTransformsVboSizeBytes = gpuBufferSizeBytes;
                    }
                    else if (gpuBufferSizeBytes > instanceData.mTransformsVboSizeBytes) {
                        //LOG_INFO("GROW {} {}", cpuBufferSizeBytes, gpuBufferSizeBytes);
                        // Grow to new size
                        GL.glDeleteBuffers(1, &instanceData.mTransformsVbo);
                        GL.glCreateBuffers(1, &instanceData.mTransformsVbo);
                        GL.glNamedBufferStorage(instanceData.mTransformsVbo, gpuBufferSizeBytes, nullptr, GL_DYNAMIC_STORAGE_BIT);
                        GL.glNamedBufferSubData(instanceData.mTransformsVbo, 0, cpuBufferSizeBytes, instanceData.mInstanceTransforms.data());
                        GL.glVertexArrayVertexBuffer(mesh.mMainMesh.mVao, BINDING_POINT, instanceData.mTransformsVbo, 0, sizeof(f32m4));
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

            GLIndirectBuffer& inDrawCommands = *instanceData.mDrawCommands;
            GLIndirectBuffer& inDrawCommandsShadows = *instanceData.mDrawCommandsShadows;
            const size_t drawCommandsSize = inDrawCommands.mDrawCommands.size();

            if (sDebugOptions.mDisableGPUCulling == false) {
                PROFILE_SCOPE("GPU Culling");
                // GPU Culling
                GpuCullUniformData uniformData;
                const f32v3& camPos = camera.getPosition();
                uniformData.cameraPos = f32v4(camPos.x, camPos.y, camPos.z, 1.0f);
                uniformData.numShapesToCull = drawCommandsSize;
                if (sDebugOptions.mDisableLOD) {
                    uniformData.lodDistancesSQ[0] = FLT_MAX;
                }
                else {
                    for (int i = 0; i < 3; ++i) {
                        uniformData.lodDistancesSQ[i] = SQ(sDebugOptions.mLodDistances[i]);
                    }
                }
                for (int i = 0; i < 4; ++i) {
                    uniformData.frustumPlanes[i] = camera.getFrustum().getPlane(i).vec4Data;
                    uniformData.lodDrawInfos[i] = mesh.mMainMesh.mLODData.getDrawInfoForLOD(MeshLODLevel(i));
                }

                mGpuCullingUniformBuffer.updateSubData(0, sizeof(GpuCullUniformData), &uniformData);
                //*instanceData.mNumVisibleMeshesBufferPtr = 0; // Compact indirect buffer is actually slower due to atomic operation and cpu-gpu sync

                mCullingComputeShader->use();
                glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
                GL.glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, instanceData.mTransformsVbo);
                GL.glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, inDrawCommands.getHandle());
                GL.glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, inDrawCommandsShadows.getHandle());
                GL.glBindBufferBase(GL_UNIFORM_BUFFER, 5, mGpuCullingUniformBuffer.getHandle());
                //glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, instanceData.mNumVisibleMeshesBuffer.getHandle());
                //glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, outDrawCommands.getHandle()); // Compact indirect buffer is actually slower due to atomic operation and cpu-gpu sync
                if (drawCommandsSize % WORK_GROUP_SIZE == 0) {
                    glDispatchCompute((GLuint)drawCommandsSize / WORK_GROUP_SIZE, 1, 1);
                }
                else {
                    glDispatchCompute(1 + (GLuint)drawCommandsSize / WORK_GROUP_SIZE, 1, 1);
                }
                glMemoryBarrier(GL_COMMAND_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT); // GL_ATOMIC_COUNTER_BARRIER_BIT
                // 114 fps
                instanceData.mShadowDrawCommandsCount = drawCommandsSize;
            }
            else {
                assert(instanceData.mInstanceTransforms.size() <= drawCommandsSize);
                PROFILE_SCOPE("CPU Culling");
                // CPU Culling
                int shadowCount = 0;
                for (size_t i = 0; i < instanceData.mInstanceTransforms.size(); ++i) {
                    DrawElementsIndirectCommand& cmd = inDrawCommands.mDrawCommands[i];
                    DrawElementsIndirectCommand& cmdShadow = inDrawCommandsShadows.mDrawCommands[shadowCount];
                    const f32m4& transform = instanceData.mInstanceTransforms[i];
                    // Columns are first
                    const f32v3& pos = reinterpret_cast<const f32v3&>(transform[3]);
                    if (camera.sphereIsVisible(pos, 10.0f)) {
                        cmd.instanceCount_ = 1;
                        cmdShadow.instanceCount_ = 1;
                        cmd.baseInstance_ = i;
                        cmdShadow.baseInstance_ = i;
                        cmd.baseVertex_ = 0;
                        cmdShadow.baseVertex_ = 0;
                        MeshLODDrawInfo drawInfo, drawInfoShadow;
                        f32 distance2 = glm::length2(pos - camera.getPosition());
                        if ((distance2 < SQ(sDebugOptions.mLodDistances[0])) || sDebugOptions.mDisableLOD) {
                            drawInfo = drawInfos[0];
                            drawInfoShadow = drawInfos[1];
                            ++shadowCount;
                        }
                        else if (distance2 < SQ(sDebugOptions.mLodDistances[1])) {
                            drawInfo = drawInfos[1];
                            drawInfoShadow = drawInfos[2];
                            ++shadowCount;
                        }
                        else if (distance2 < SQ(sDebugOptions.mLodDistances[2])) {
                            drawInfo = drawInfos[2];
                            drawInfoShadow = drawInfos[3];
                            ++shadowCount;
                        }
                        else {
                            drawInfo = drawInfos[3];
                            drawInfoShadow = drawInfos[3];
                        }
                        cmd.count_ = drawInfo.indexCount;
                        cmd.firstIndex_ = drawInfo.startIndex;
                        cmdShadow.count_ = drawInfoShadow.indexCount;
                        cmdShadow.firstIndex_ = drawInfoShadow.startIndex;
                    }
                    else {
                        cmd.instanceCount_ = 0;
                    }
                }
                instanceData.mShadowDrawCommandsCount = shadowCount;
                inDrawCommands.uploadIndirectBuffer();
                inDrawCommandsShadows.uploadIndirectBuffer();

            }

            // Compact indirect buffer is actually slower due to atomic operation and cpu-gpu sync
            // Sync start of draw
           /* if (inDrawCommands.mDrawCommands.size()) {
                glMemoryBarrier(GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT);
                instanceData.mFenceSync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
            }*/

        }
    }
}

void InstancedStaticModelManager::addInstanceAtPosition(TileContainerID containerId, TileIndex tileIndex, ModelID modelId, const f32v3& position, f32 rotation)
{
    ASSERT_RENDER_THREAD();
    const int ri = getRenderPassIndexForModel(modelId);
    StaticModelInstanceData& instanceData = mModelsToInstances[ri][modelId];

    const size_t instanceIndex = instanceData.mInstanceTransforms.size();
    if (instanceIndex < instanceData.mFirstDirtyInstance) {
        instanceData.mFirstDirtyInstance = instanceIndex;
    }
    // Store per tile references
    instanceData.mInstanceTransforms.emplace_back(ModelUtil::computeTransformMatrixForModel(position, rotation));
    instanceData.mInstanceOwners.emplace_back(ModelInstanceOwner{ containerId, tileIndex });
    TileModelPositionKey positionKey{ tileIndex };

    SpatialInstanceDataMap& tileContainerModels = mTileContainerModels[containerId];
    assert(tileContainerModels.find(positionKey) == tileContainerModels.end());
    tileContainerModels[positionKey] = { modelId, (ui32)instanceIndex };
}

void InstancedStaticModelManager::removeInstanceAtPosition(TileContainerID containerId, TileIndex tileIndex)
{
    ASSERT_RENDER_THREAD();
    auto&& it = mTileContainerModels.find(containerId);
    if (it != mTileContainerModels.end()) {
        TileModelPositionKey key{ tileIndex };
        SpatialInstanceDataMap& spatialMap = it->second;
        auto&& spit = spatialMap.find(key);

        if (spit != spatialMap.end()) {
            removeTileModelInstanceInternal(spit->second);
            spatialMap.erase(spit);
            if (spatialMap.empty()) {
                mTileContainerModels.erase(it);
            }
        }
    }
}

void InstancedStaticModelManager::addInstancesFromGatherer(InstancedStaticModelGatherer& gatherer)
{
    ASSERT_RENDER_THREAD();
    PROFILE_FUNCTION();
    if (gatherer.mInstances.empty()) {
        return;
    }
    // OLD: Gatherer should only be used once for init, and future updates should be done per tile
    // Remove all instances before we add new ones
    if (mTileContainerModels.find(gatherer.mContainerID) != mTileContainerModels.end()) {
        removeInstancesFromContainer(gatherer.mContainerID);
    }
    SpatialInstanceDataMap& tileContainerModels = mTileContainerModels[gatherer.mContainerID];
    for (auto&& it : gatherer.mInstances) {
        // Insert all instance transforms ordered into the transforms array
        const std::vector<StaticModelInstance>& sourceInstances = it.second;
        const int ri = getRenderPassIndexForModel(it.first);
        StaticModelInstanceData& instanceData = mModelsToInstances[ri][it.first];
        const size_t startIndex = instanceData.mInstanceTransforms.size();
        // Track where our buffer is dirty
        if (startIndex < instanceData.mFirstDirtyInstance) {
            instanceData.mFirstDirtyInstance = startIndex;
        }
        instanceData.mInstanceTransforms.resize(startIndex + sourceInstances.size());
        instanceData.mInstanceOwners.resize(instanceData.mInstanceTransforms.size());
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

void InstancedStaticModelManager::removeInstancesFromContainer(TileContainerID containerId)
{
    // TODO: There is a race condition if the tile container is being meshed. Make sure we only destroy tile containers once they are done
    // being meshed?
    ASSERT_RENDER_THREAD();
    auto&& it = mTileContainerModels.find(containerId);
    if (it == mTileContainerModels.end()) {
        return;
    }
    SpatialInstanceDataMap& tileContainerModels = it->second;
    for (auto& it : tileContainerModels) {
        removeTileModelInstanceInternal(it.second);
    }
    mTileContainerModels.erase(it);
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

void InstancedStaticModelManager::onContainerEditEvent(const TileContainerEvent& evnt)
{
    ASSERT_GAME_THREAD();

    if ((e_cast(evnt.edit.type) & MODEL_EDIT_HANDLE_MASK) == 0) {
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

    switch (evnt.edit.type) {
        case TileContainerEditEventType::ChangeFlags:
            break;
        case TileContainerEditEventType::ChangeLayer: {
            for (ui32 i = 0; i < evnt.edit.editCount; ++i) {
                TileContainerEditLayerEventData& edit = evnt.edit.changeLayerArray[i];
                const TileID prevId = edit.prevId;
                if (prevId != TILE_ID_NONE) {
                    const TileData& prevTileData = TileRepository::getTileData(edit.prevId);
                    if (prevTileData.shape == TileShape::MODEL) {
                        editEvents.removeEvents.emplace_back(edit.tileIndex);
                    }
                }
                const TileID newId = edit.newId;
                assert(newId != prevId);
                if (newId != TILE_ID_NONE) {
                    const TileData& tileData = TileRepository::getTileData(newId);
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

void InstancedStaticModelManager::removeTileModelInstanceInternal(TileModelInstance& instance)
{
    const int ri = getRenderPassIndexForModel(instance.mModelID);
    auto&& it = mModelsToInstances[ri].find(instance.mModelID);
    assert(it != mModelsToInstances[ri].end());
    StaticModelInstanceData& instanceData = it->second;
    const ui32 instanceIndex = instance.mInstanceIndex;
    if (instanceIndex < instanceData.mFirstDirtyInstance) {
        instanceData.mFirstDirtyInstance = instanceIndex;
    }

    // Tell back owner about new position by grabbing transform position to look up
    ModelInstanceOwner backOwner = instanceData.mInstanceOwners.back();
    auto&& it2 = mTileContainerModels.find(backOwner.containerId);
    assert(it2 != mTileContainerModels.end());
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
        mModelsToInstances[ri].erase(it);
    }
}
