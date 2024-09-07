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
#include "rendering/model/ModelImpostorManager.h"

#include "camera/Camera3D.h"

#include <boost/pool/singleton_pool.hpp>

#include "options/DebugOptions.h"

#include "rendering/gl/GL.h"

//#include <glm/gtx/matrix_decompose.hpp>

// Types of events we handle
constexpr ui8 MODEL_EDIT_HANDLE_MASK = e_cast(TileContainerEditEventType::ChangeZPos) | e_cast(TileContainerEditEventType::ChangeTileID) | e_cast(TileContainerEditEventType::ChangeOrientation);
static_assert(e_cast(TileContainerEditEventType::TERM) == BIT(4), "Update handler");

// Water not supported
std::array<MaterialRenderPassType, 2> CROSSFADE_PASSES = { MaterialRenderPassType::Default, MaterialRenderPassType::Smudge };
static_assert(e_count(MaterialRenderPassType) == 3);

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

// Maximum concurrent LOD transitioning meshes, note that an impostor doesn't count towards this
constexpr int MAX_LOD_DITHER_TRANSITION_DRAWS = 2048;
constexpr int MAX_TRANSFORMATION_TRANSITION_DRAWS = 512;
constexpr int WORK_GROUP_SIZE = 64;
constexpr f32 LOD_TRANSITION_SPEED = 1.0f; // Multiplied by elapsedSec. 1 = 1 second, 2 = 0.5 seconds
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
    ASSERT_GAME_THREAD(); // This is currently created on the game thread

    mCullingComputeShader = MaterialShaderRepository::get().getAssetHandle(CStrToken("culling_and_lod"));
    mBillboardLodManager = std::make_unique<ModelImpostorManager>(ModelRepository::get().getImpostorRepository());
    
    constexpr size_t RESERVE_COUNT = 1024;
    mInstanceTransforms.reserve(RESERVE_COUNT);
    mInstanceGpuData.reserve(RESERVE_COUNT);
    mInstanceSources.reserve(RESERVE_COUNT);
    mInstanceDrawData.reserve(RESERVE_COUNT);
    mModelDamageZonesGpuData.emplace_back();
}

InstancedStaticModelManager::~InstancedStaticModelManager() {

    GL.glDeleteBuffers(1, &mTransformsVbo);
    GL.glDeleteBuffers(1, &mInstanceDataVbo);
    GL.glDeleteBuffers(1, &mDamageZonesSSBO);

}

void InstancedStaticModelManager::frameUpdate(const Camera3D& camera, f32 elapsedSec) {
    ASSERT_RENDER_THREAD();
    if (sDebugOptions.mHideModels)
        return;

    if (mNeedsInit) {
        init();
    }

    PROFILE_FUNCTION();

    mBillboardLodManager->frameBegin();

    updatePendingLooseModelInstances();

    const int RENDER_PASS_COUNT = e_count(MaterialRenderPassType);

    // Build GPU data and cull
       
    // TODO: Move this to onRemove
    if (!mInstanceTransforms.size()) [[unlikely]] {
        for (int r = 0; r < RENDER_PASS_COUNT; ++r) {
            mDrawCommands[r].reset();
            mDrawCommandsShadows[r].reset();
        }
        if (mTransformsVbo) {
            GL.glDeleteBuffers(1, &mTransformsVbo);
            GL.glDeleteBuffers(1, &mInstanceDataVbo);
            GL.glDeleteBuffers(1, &mDamageZonesSSBO);
            mTransformsVbo = 0;
            mInstanceDataVbo = 0;
            mDamageZonesSSBO = 0;
        }
        return;
    }
       
    if (mFirstDirtyInstance != UINT32_MAX) {
        PROFILE_SCOPE("Rebuild Indirect Buffer");
        // Rebuild command buffer
        {
            PROFILE_SCOPE("Indirect Buffer");

            constexpr ui32 MAX_PADDING_BEFORE_SHRINK = 256;
            // Grow or shrink the command buffers when needed, which should be rare as we keep padding
            for (int r = 0; r < RENDER_PASS_COUNT; ++r) {
                std::unique_ptr<GLDrawCommandBuffer>& drawCommands = mDrawCommands[r];
                std::unique_ptr<GLDrawCommandBuffer>& drawCommandsShadows = mDrawCommandsShadows[r];
                // Keep space for dither transitions
                const ui32 drawCommandsCount = mDrawCommandsCount[r] + MAX_LOD_DITHER_TRANSITION_DRAWS;
                const ui32 drawCommandsShadowsCount = mDrawCommandsShadowsCount[r];
                if (drawCommandsCount) {
                    if (!drawCommands ||
                        (drawCommands->getCapacity() < drawCommandsCount) ||
                        (drawCommands->getCapacity() > (drawCommandsCount - MAX_PADDING_BEFORE_SHRINK))) {
                        mDrawCommands[r] = std::make_unique<GLDrawCommandBuffer>(mDrawCommandsCount[r] + (MAX_PADDING_BEFORE_SHRINK / 2));
                    }
                    if (drawCommandsShadowsCount) {
                        if (!drawCommandsShadows ||
                            (drawCommandsShadows->getCapacity() < drawCommandsShadowsCount) ||
                            (drawCommandsShadows->getCapacity() > (drawCommandsShadowsCount - MAX_PADDING_BEFORE_SHRINK))) {
                            drawCommandsShadows = std::make_unique<GLDrawCommandBuffer>(mDrawCommandsShadowsCount[r] + (MAX_PADDING_BEFORE_SHRINK / 2));
                        }
                    } else {
                        drawCommandsShadows.reset();
                    }
                }
                else {
                    drawCommands.reset();
                    drawCommandsShadows.reset();
                }
            }
        }

        // TODO: Is this needed? Research
        //const size_t workGroupRoundedSize = roundToWorkGroupSize(mInstanceTransforms.size());
        const size_t numInstances = mInstanceTransforms.size();
        // Allocate VBO
        {
            PROFILE_SCOPE("VBO");
            assert(mInstanceGpuData.size() == mInstanceTransforms.size());
            if (numInstances > mInstancesCapacity) {
                constexpr i32 CAPACITY_PADDING = 256; // Prevent many reallocations
                mInstancesCapacity = numInstances + CAPACITY_PADDING;

                // Grow to new size
                GL.glDeleteBuffers(1, &mTransformsVbo); // Deleting 0 is safe
                GL.glDeleteBuffers(1, &mInstanceDataVbo);
                GL.glDeleteBuffers(1, &mDamageZonesSSBO);
                GL.glCreateBuffers(1, &mTransformsVbo);
                GL.glCreateBuffers(1, &mInstanceDataVbo);
                GL.glCreateBuffers(1, &mDamageZonesSSBO);
                GL.glNamedBufferStorage(mTransformsVbo, sizeof(f32m4) * mInstancesCapacity, nullptr, GL_DYNAMIC_STORAGE_BIT);
                GL.glNamedBufferSubData(mTransformsVbo, 0, sizeof(f32m4) * numInstances, mInstanceTransforms.data());
                GL.glNamedBufferStorage(mInstanceDataVbo, sizeof(InstanceGpuData) * mInstancesCapacity, nullptr, GL_DYNAMIC_STORAGE_BIT);
                GL.glNamedBufferSubData(mInstanceDataVbo, 0, sizeof(InstanceGpuData) * numInstances, mInstanceGpuData.data());
                // TODO: we don't always need to recreate the damage zones SSBO...
                GL.glNamedBufferStorage(mDamageZonesSSBO, sizeof(ModelDamageZoneGpuData) * mModelDamageZonesGpuData.size(), mModelDamageZonesGpuData.data(), GL_DYNAMIC_STORAGE_BIT);
            }
            else {
                // Note that we never shrink the buffer if its too big. Probably fine.
                assert(mLastDirtyInstance >= mFirstDirtyInstance);
                const ui32 totalDirty = mLastDirtyInstance - mFirstDirtyInstance + 1;
                // Only upload data between the dirty instances, which should amortize things a bit though worst case is still full upload
                //   If this shows up in profiling, we can store a list of dirty instances and just upload those unless there is more than N
                glNamedBufferSubData(
                    mTransformsVbo,
                    mFirstDirtyInstance * sizeof(f32m4),
                    totalDirty * sizeof(f32m4),
                    mInstanceTransforms.data() + mFirstDirtyInstance
                );
                glNamedBufferSubData(
                    mInstanceDataVbo,
                    mFirstDirtyInstance * sizeof(InstanceGpuData),
                    totalDirty * sizeof(InstanceGpuData),
                    mInstanceGpuData.data() + mFirstDirtyInstance
                );

                // Refresh all damage zones except the first one every time
                // TODO: We likely only need to do this if a damage zone was added or removed
                glNamedBufferSubData(
                    mDamageZonesSSBO,
                    sizeof(ModelDamageZoneGpuData), // Skip first
                    sizeof(ModelDamageZoneGpuData) * (mModelDamageZonesGpuData.size() - 1),
                    mModelDamageZonesGpuData.data() + 1
                );
            }
        }

        mFirstDirtyInstance = UINT32_MAX;
        mLastDirtyInstance = 0;
    }

    for (int r = 0; r < RENDER_PASS_COUNT; ++r) {
        if (mDrawCommandsCount[r]) {
            assert(mDrawCommands[r]);
            mDrawCommands[r]->frameBegin();
            if (mDrawCommandsShadowsCount[r]) {
                assert(mDrawCommandsShadows[r]);
                mDrawCommandsShadows[r]->frameBegin();
            }
        }
    }
    if (sDebugOptions.mDisableGPUCulling == false) {
        PROFILE_SCOPE("GPU Culling");
        // GPU Culling
        panic("GPU Culling is defunct");
    }
    else {
        PROFILE_SCOPE("CPU Culling");
        // CPU Culling
        // TODO: Should the renderer handle this??
        int activeCount[e_count(MaterialRenderPassType)] = {};
        int shadowCount[e_count(MaterialRenderPassType)] = {};
        int crossfadeActiveCount[e_count(MaterialRenderPassType)] = {};
        crossfadeActiveCount[e_cast(MaterialRenderPassType::Water)] = INT32_MAX;
        f32* crossfadeArrays[e_count(MaterialRenderPassType)] = {};
        // Ignoring water
        for (MaterialRenderPassType type : CROSSFADE_PASSES) {
            crossfadeArrays[e_cast(type)] = static_cast<f32*>(mCrossfadeBuffers[e_cast(type)]->frameBeginAndGetDataForUpdate());
            mDrawCommandsCrossfade[e_cast(type)]->frameBegin();
        }
        static_assert(e_count(MaterialRenderPassType) == 3);

        auto setCommand = [](
            DrawElementsIndirectCommand& cmd, GLuint transformIndex, ui32 baseVertex, const MeshLODDrawInfo& drawInfo) {
            cmd.count_ = drawInfo.indexCount;
            cmd.instanceCount_ = 1;
            cmd.firstIndex_ = drawInfo.startIndex;
            cmd.baseVertex_ = baseVertex;
            cmd.baseInstance_ = transformIndex; // * 2 because we potentially have 2 instances per transform if we are crossfading
        };

        auto addBillboard = [this](const InstanceDrawData& instanceDrawData, f32v3 pos, ui32 instanceIndex, f32 crossfade = -MATH_EPSILON) {
            const ModelDef& modelDef = ModelRepository::get().getLoadedOrUnloadedAsset(instanceDrawData.modelId);
            const f32AABB3& aabb = modelDef.mAABB;
            const f32 scale = mInstanceScales[instanceIndex];
            mBillboardLodManager->addBillboard(instanceDrawData.modelId, f32v3(pos.x, pos.y, pos.z + aabb.z * scale), f32v2(aabb.width * scale, aabb.height * scale), crossfade);
        };

        auto addCrossfadingModel = [this, &addBillboard, &activeCount, &crossfadeActiveCount, &setCommand, &crossfadeArrays](
            MeshLODLevel lod,
            const InstanceDrawData& instanceDrawData,
            const ModelBatchSubmeshDrawData* drawDataArray,
            f32v3 pos, ui32 instanceIndex, f32 crossfade
        ) {
            if (lod == MeshLODLevel::IMPOSTOR) {
                addBillboard(instanceDrawData, pos, instanceIndex, crossfade);
            }
            else {
                for (int m = 0; m < instanceDrawData.key.count; ++m) {
                    const ModelBatchSubmeshDrawData& drawData = drawDataArray[m];
                    const int renderPassIndex = e_cast(drawData.renderPass);
                    int& cActive = crossfadeActiveCount[renderPassIndex];
                    if (cActive < MAX_LOD_DITHER_TRANSITION_DRAWS) {
                        crossfadeArrays[renderPassIndex][cActive] = crossfade;
                        setCommand(mDrawCommandsCrossfade[renderPassIndex]->getDrawCommands().data()[cActive++], (GLuint)instanceIndex, drawData.baseVertex, drawData.lodDrawInfo[e_cast(lod)]);
                    }
                    else {
                        // Fallback to normal render if we are out of space, but only for the fading in model
                        if (crossfade > 0.0) {
                            setCommand(mDrawCommands[renderPassIndex]->getDrawCommands().data()[activeCount[renderPassIndex]++], (GLuint)instanceIndex, drawData.baseVertex, drawData.lodDrawInfo[e_cast(lod)]);
                        }
                    }
                }
            }
        };

        auto getDesiredLOD = [&camera](const f32 distance2, const ModelLodParams& lodParams) -> MeshLODLevel{
            if (distance2 >= lodParams.lodDistancesSQ[3] || sDebugOptions.mForceImpostors) {
                return MeshLODLevel::IMPOSTOR;
            }
            else if (distance2 >= lodParams.lodDistancesSQ[2]) {
                return MeshLODLevel::Lowest;
            }
            else if (distance2 >= lodParams.lodDistancesSQ[1]) {
                return MeshLODLevel::Low;
            }
            else if (distance2 >= lodParams.lodDistancesSQ[0]) {
                return MeshLODLevel::Medium;
            }
            return MeshLODLevel::Highest;
        };

        std::vector<TileModelInstanceIndex> transformationIndicesToRemove;

        const f32 LodTransitionSpeed = LOD_TRANSITION_SPEED * sDebugOptions.mLodCrossfadeSpeed;
        const f32 TransformationSpeed = LOD_TRANSITION_SPEED; // TODO: Dynamic?

        ModelRepository& modelRepo = ModelRepository::get();
        for (TileModelInstanceIndex instanceIndex = 0; instanceIndex < (TileModelInstanceIndex)mInstanceTransforms.size(); ++instanceIndex) {
            const f32m4& transform = mInstanceTransforms[instanceIndex];
            // Columns are first
            const f32v3& pos = reinterpret_cast<const f32v3&>(transform[3]);
            const InstanceDrawData& instanceDrawData = mInstanceDrawData[instanceIndex];
            InstanceCrossfadeData& transitionData = mInstanceCrossfadeData[instanceIndex];
            const ModelLodParams& lodParams = modelRepo.getLodParams(instanceDrawData.modelId);

            if (transitionData.isActive()) {
                if (transitionData.mTransformationDataIndex != INVALID_TRANSFORMATION_DATA_INDEX) {
                    TransformationData& transformationData = mTransformationData[transitionData.mTransformationDataIndex];
                    // Transformation transition updates, such as corruption
                    if (transformationData.isFrom) {
                        transitionData.mCrossfade -= TransformationSpeed * elapsedSec;
                    }
                    else {
                        transitionData.mCrossfade += TransformationSpeed * elapsedSec;
                    }
                    if (std::fabs(transitionData.mCrossfade) >= 1.0f) {

                        assert(mNumActiveTransformationTransitions > 0);
                        --mNumActiveTransformationTransitions;

                        transitionData.mCrossfade = 0.0f;

                        // Add from index to remove later:
                        transitionData.mTransformationDataIndex = INVALID_TRANSFORMATION_DATA_INDEX;
                        if (transformationData.isFrom) {
                            assert(transformationData.fromIndex == instanceIndex);
                            transformationIndicesToRemove.push_back(transformationData.fromIndex);
                            continue;
                        }
                    }
                    else {
                        if (camera.sphereIsVisible(pos, lodParams.boundingSphereRadius)) {
                            const ModelBatchSubmeshDrawData* drawDataArray = modelRepo.getSubmeshDrawDataArrayForModel(instanceDrawData.key);

                            addCrossfadingModel(transitionData.mCurrentLOD, instanceDrawData, drawDataArray, pos, instanceIndex, transitionData.mCrossfade);

                            // DrawShadows for whichever is closer
                            if ((transformationData.isFrom && transitionData.mCrossfade > -0.5f) || (!transformationData.isFrom && transitionData.mCrossfade >= 0.5f)) {
                                // Draw current shadow
                                if ((int)lodParams.shadowLodDetail > (int)transitionData.mCurrentLOD) {
                                    const ModelBatchSubmeshDrawData* drawDataArray = modelRepo.getSubmeshDrawDataArrayForModel(instanceDrawData.key);
                                    for (int m = 0; m < instanceDrawData.key.count; ++m) {
                                        const ModelBatchSubmeshDrawData& drawData = drawDataArray[m];
                                        const int renderPassIndex = e_cast(drawData.renderPass);
                                        if (drawData.castsShadow) {
                                            setCommand(mDrawCommandsShadows[renderPassIndex]->getDrawCommands().data()[shadowCount[renderPassIndex]++], (GLuint)instanceIndex, drawData.baseVertex, drawData.lodDrawInfo[(int)transitionData.mCurrentLOD]);
                                        }
                                    }
                                }
                            }
                        }

                        continue;
                    }
                }
                else {
                    // LOD Dither transitions
                    transitionData.mCrossfade += LodTransitionSpeed * elapsedSec;
                    // Stop if we are finished
                    if (transitionData.mCrossfade >= 1.0f) {
                        transitionData.mCrossfade = 0.0f;
                        transitionData.mCurrentLOD = transitionData.mTargetLOD;
                        assert(mNumActiveLodTransitions > 0);
                        --mNumActiveLodTransitions;
                    }
                    else {
                        if (camera.sphereIsVisible(pos, lodParams.boundingSphereRadius)) {
                            const ModelBatchSubmeshDrawData* drawDataArray = modelRepo.getSubmeshDrawDataArrayForModel(instanceDrawData.key);
                            const f32 sourceCrossfade = -transitionData.mCrossfade;
                            const f32 targetCrossfade = transitionData.mCrossfade;

                            addCrossfadingModel(transitionData.mCurrentLOD, instanceDrawData, drawDataArray, pos, instanceIndex, sourceCrossfade);
                            addCrossfadingModel(transitionData.mTargetLOD, instanceDrawData, drawDataArray, pos, instanceIndex, targetCrossfade);

                            // DrawShadows for whichever is closer
                            if (targetCrossfade < 0.5f) {
                                // Draw current shadow
                                if ((int)lodParams.shadowLodDetail > (int)transitionData.mCurrentLOD) {
                                    for (int m = 0; m < instanceDrawData.key.count; ++m) {
                                        const ModelBatchSubmeshDrawData& drawData = drawDataArray[m];
                                        const int renderPassIndex = e_cast(drawData.renderPass);
                                        if (drawData.castsShadow) {
                                            setCommand(mDrawCommandsShadows[renderPassIndex]->getDrawCommands().data()[shadowCount[renderPassIndex]++], (GLuint)instanceIndex, drawData.baseVertex, drawData.lodDrawInfo[(int)transitionData.mCurrentLOD]);
                                        }
                                    }
                                }
                            }
                            else {
                                // Draw target shadow
                                if ((int)lodParams.shadowLodDetail > (int)transitionData.mTargetLOD) {
                                    for (int m = 0; m < instanceDrawData.key.count; ++m) {
                                        const ModelBatchSubmeshDrawData& drawData = drawDataArray[m];
                                        const int renderPassIndex = e_cast(drawData.renderPass);
                                        if (drawData.castsShadow) {
                                            setCommand(mDrawCommandsShadows[renderPassIndex]->getDrawCommands().data()[shadowCount[renderPassIndex]++], (GLuint)instanceIndex, drawData.baseVertex, drawData.lodDrawInfo[(int)transitionData.mTargetLOD]);
                                        }
                                    }
                                }
                            }
                        }
                        else {
                            // Offscreen can instantly finish crossfading
                            transitionData.mCrossfade = 0.0f;
                            transitionData.mCurrentLOD = transitionData.mTargetLOD;
                            assert(mNumActiveLodTransitions > 0);
                            --mNumActiveLodTransitions;
                        }
                        continue;
                    }
                }
            }

            // TODO: Real bounding sphere
            if (camera.sphereIsVisible(pos, lodParams.boundingSphereRadius)) {
                const f32 distance2 = glm::length2(pos - camera.getPosition());
                const ModelBatchSubmeshDrawData* drawDataArray = modelRepo.getSubmeshDrawDataArrayForModel(instanceDrawData.key);

                // TODO: Remove
                if (sDebugOptions.mDisableLOD) [[unlikely]] {
                    for (int m = 0; m < instanceDrawData.key.count; ++m) {
                        const ModelBatchSubmeshDrawData& drawData = drawDataArray[m];
                        const int renderPassIndex = e_cast(drawData.renderPass);
                        setCommand(mDrawCommands[renderPassIndex]->getDrawCommands()[activeCount[renderPassIndex]++], instanceIndex, drawData.baseVertex, drawData.lodDrawInfo[0]);

                        if (drawData.castsShadow) {
                            setCommand(mDrawCommandsShadows[renderPassIndex]->getDrawCommands()[shadowCount[renderPassIndex]++], instanceIndex, drawData.baseVertex, drawData.lodDrawInfo[0]);
                        }
                    }
                    continue;
                }

                const MeshLODLevel desiredLod = getDesiredLOD(distance2, lodParams);

                // Only happens the first time
                if (transitionData.mCurrentLOD == MeshLODLevel::INVALID) [[unlikely]] {
                    transitionData.mCurrentLOD = desiredLod;
                }
                if (transitionData.mCurrentLOD == MeshLODLevel::IMPOSTOR || sDebugOptions.mForceImpostors) {
                    addBillboard(instanceDrawData, pos, instanceIndex);
                }
                else {
                    for (int m = 0; m < instanceDrawData.key.count; ++m) {
                        const ModelBatchSubmeshDrawData& drawData = drawDataArray[m];
                        const int renderPassIndex = e_cast(drawData.renderPass);
                        setCommand(mDrawCommands[renderPassIndex]->getDrawCommands().data()[activeCount[renderPassIndex]++], (GLuint)instanceIndex, drawData.baseVertex, drawData.lodDrawInfo[(int)transitionData.mCurrentLOD]);
                    }
                    if ((int)lodParams.shadowLodDetail > (int)transitionData.mCurrentLOD) {
                        for (int m = 0; m < instanceDrawData.key.count; ++m) {
                            const ModelBatchSubmeshDrawData& drawData = drawDataArray[m];
                            const int renderPassIndex = e_cast(drawData.renderPass);
                            if (drawData.castsShadow) {
                                setCommand(mDrawCommandsShadows[renderPassIndex]->getDrawCommands().data()[shadowCount[renderPassIndex]++], (GLuint)instanceIndex, drawData.baseVertex, drawData.lodDrawInfo[(int)transitionData.mCurrentLOD]);
                            }
                        }
                    }
                }

                transitionData.mTargetLOD = desiredLod;
                // Enable LOD transition
                if (transitionData.mCurrentLOD != transitionData.mTargetLOD) {
                    transitionData.mCrossfade = MATH_EPSILON;
                    ++mNumActiveLodTransitions;
                }
            }
        }

        for (MaterialRenderPassType type : CROSSFADE_PASSES) {
            mCrossfadeBuffers[e_cast(type)]->flushDataAndIncrementFrame(crossfadeActiveCount[e_cast(type)]);
            if (crossfadeActiveCount[e_cast(type)] > 0) {
                LOG_INFO("Crossfade count: {} - {}", e_cast(type), crossfadeActiveCount[e_cast(type)]);
            }
            mDrawCommandsCrossfade[e_cast(type)]->setNumActiveCommands(crossfadeActiveCount[e_cast(type)]);
            mDrawCommandsCrossfade[e_cast(type)]->uploadDrawCommands();
        }

        // TODO: We lose a lot of these due to culling, so we probably don't need
        // the draw command capacity to be so high
        for (int r = 0; r < RENDER_PASS_COUNT; ++r) {
            if (mDrawCommands[r]) {
                assert(activeCount[r] <= mDrawCommandsCount[r]);
                mDrawCommands[r]->setNumActiveCommands(activeCount[r]);
                mDrawCommands[r]->uploadDrawCommands();
                if (mDrawCommandsShadows[r]) {
                    assert(shadowCount[r] <= mDrawCommandsShadowsCount[r]);
                    mDrawCommandsShadows[r]->setNumActiveCommands(shadowCount[r]);
                    mDrawCommandsShadows[r]->uploadDrawCommands();
                }

            }
        }

        // Remove transformations that finished
        for (TileModelInstanceIndex index : transformationIndicesToRemove) {
            removeModelInstanceInternal(index);
        }
    }

    mBillboardLodManager->flushDataAndIncrementFrame();

    updateAnimatedModels(elapsedSec);
}

void InstancedStaticModelManager::addTileInstanceAtPosition(TileContainerID containerId, TileIndex tileIndex, ModelID modelId, f32v3 position, f32 rotation, ui8 variantIndex, TileDamageDataPtr damageData, f32 scale) {
    ASSERT_RENDER_THREAD();

    increfModelDef(modelId, 1);
    
    const ModelDef& modelDef = ModelRepository::get().getLoadedOrUnloadedAsset(modelId);

    addTileInstanceInternal(modelDef, containerId, tileIndex, ModelUtil::computeTransformMatrixForModel(position, rotation, scale), variantIndex, std::move(damageData), scale);
}

void InstancedStaticModelManager::removeTileInstanceAtPosition(TileContainerID containerId, TileIndex tileIndex) {
    ASSERT_RENDER_THREAD();

    auto&& it = mTileContainerTrackedModels.find(containerId);
    if (it != mTileContainerTrackedModels.end()) {
        SpatialInstanceDataMap& spatialMap = it->second;
        auto&& spit = spatialMap.find(tileIndex);

        if (spit != spatialMap.end()) {
            removeModelInstanceInternal(spit->second);
            spatialMap.erase(spit);
            if (spatialMap.empty()) {
                mTileContainerTrackedModels.erase(it);
            }
        }
    }
}

TileModelInstanceIndex InstancedStaticModelManager::getTileInstanceIndexAtPosition(LiteTileHandle tileHandle) {
    ASSERT_RENDER_THREAD();
    auto&& it = mTileContainerTrackedModels.find(tileHandle.containerId);
    if (it != mTileContainerTrackedModels.end()) {
        SpatialInstanceDataMap& spatialMap = it->second;
        auto&& spit = spatialMap.find(tileHandle.index);

        if (spit != spatialMap.end()) {
            return spit->second;
        }
    }
    return INVALID_TILE_MODEL_INSTANCE_INDEX;
}

bool InstancedStaticModelManager::hasTileInstanceAtPosition(LiteTileHandle tileHandle, ModelID modelId) {
    auto&& it = mTileContainerTrackedModels.find(tileHandle.containerId);
    if (it != mTileContainerTrackedModels.end()) {
        SpatialInstanceDataMap& spatialMap = it->second;
        auto&& spit = spatialMap.find(tileHandle.index);

        if (spit != spatialMap.end()) {
            return mInstanceDrawData[spit->second].modelId == modelId;
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
    // TODO: We should never do this as instead we should handle model changes directly
    removeTileInstancesFromContainer(gatherer.mContainerID);

    ModelRepository& modelRepo = ModelRepository::get();
    for (auto&& it : gatherer.mInstances) {

        ModelID modelId = it.first;
        const std::vector<StaticModelInstance>& sourceInstances = it.second;

        ModelBatchSubmeshDrawDataSpanKey drawDataKey = ModelRepository::get().getDrawDataSpanKeyForModel(modelId);

        increfModelDef(modelId, sourceInstances.size());

        SpatialInstanceDataMap& tileContainerModels = mTileContainerTrackedModels[gatherer.mContainerID];

        const VariantIndexData variantData = modelRepo.getVariantArrayIndexDataForModel(modelId);

        const size_t startIndex = mInstanceTransforms.size();
        mInstanceTransforms.resize(startIndex + sourceInstances.size());
        mInstanceScales.resize(mInstanceTransforms.size());
        mInstanceGpuData.resize(mInstanceTransforms.size());
        mInstanceSources.resize(mInstanceTransforms.size());
        mInstanceDrawData.resize(mInstanceTransforms.size());
        mInstanceCrossfadeData.resize(mInstanceTransforms.size());

        // Dirty the beginning of range
        onDirtyModelInstance(startIndex);
        // Dirty the end of range
        onDirtyModelInstance(mInstanceTransforms.size() - 1);

        // Store per tile references
        for (size_t i = 0; i < sourceInstances.size(); ++i) {
            const size_t instanceIndex = startIndex + i;
            const StaticModelInstance& modelInstance = sourceInstances[i];
            mInstanceTransforms[instanceIndex] = modelInstance.matrix;
            mInstanceGpuData[instanceIndex].submeshDataIndex = drawDataKey.startIndex;
            mInstanceGpuData[instanceIndex].variantIndex = variantData.offset + (InstanceVariantIndexType)modelInstance.variantIndex * variantData.stride;
            mInstanceSources[instanceIndex] = ModelInstanceContainerOwner{ gatherer.mContainerID, modelInstance.tileIndex };
            if (modelInstance.damageData) {
                mInstanceGpuData[instanceIndex].damageModelIndex = mModelDamageZonesGpuData.size();
                ModelDamageZoneGpuData& gpuDamageData = mModelDamageZonesGpuData.emplace_back();
                gpuDamageData.damageZones = modelInstance.damageData->getShellDamageZones();
            }
            else {
                mInstanceGpuData[instanceIndex].damageModelIndex = 0;
            }
            mInstanceDrawData[instanceIndex] = InstanceDrawData(drawDataKey, modelId);
            mInstanceScales[instanceIndex] = modelInstance.scale;
            incrementDrawCommandsCount(drawDataKey);

            assert(tileContainerModels.find(modelInstance.tileIndex) == tileContainerModels.end());
            tileContainerModels[modelInstance.tileIndex] = { (ui32)instanceIndex };
        }
    }
}

void InstancedStaticModelManager::removeTileInstancesFromContainer(TileContainerID containerId) {
    // TODO: There is a race condition if the tile container is being meshed. Make sure we only destroy tile containers once they are done
    // being meshed?
    ASSERT_RENDER_THREAD();

    auto&& it = mTileContainerTrackedModels.find(containerId);
    if (it != mTileContainerTrackedModels.end()) {
        SpatialInstanceDataMap& tileContainerModels = it->second;
        for (auto& it2 : tileContainerModels) {
            removeModelInstanceInternal(it2.second);
        }
        mTileContainerTrackedModels.erase(it);
    }
}

ui32 InstancedStaticModelManager::getNumModels() const {
    ASSERT_RENDER_THREAD();
    return mInstanceTransforms.size();
}

bool InstancedStaticModelManager::playAnimationOnInstanceAtPosition(LiteTileHandle targetTile, StaticModelAnimationTypes animType, f32v2 direction, ModelID modelId) {
    ASSERT_RENDER_THREAD();
    // Ensure tile exists as static model
    if (!hasTileInstanceAtPosition(targetTile, modelId)) {
        return false;
    }

    // Overwrite existing animation if any
    StaticMeshAnimation& anim = mAnimatedTileInstances[targetTile];
    anim.animType = animType;
    anim.currentTimeSec = 0.0f;
    anim.direction = direction;
    return true;
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
        f32 scale;
    };
    struct ModelHeightAdjustEvent {
        f32 zPos;
        TileIndex tileIndex;
    };
    struct ModelTransformEvent {
        ModelID nextId;
        TileIndex tileIndex;
        ui8 nextVariant;
        TileTransformationType transformType;
    };
    struct ModelEditEvents {
        TileContainerID containerId;
        InstancedStaticModelManager* manager = nullptr;
        std::vector<ModelAddEvent> addEvents;
        std::vector<ModelHeightAdjustEvent> heightAdjustEvents;
        std::vector<TileIndex> removeEvents;
        std::vector<ModelTransformEvent> transformEvents;

        bool hasAny() const {
            return removeEvents.size() || addEvents.size() || heightAdjustEvents.size() || transformEvents.size();
        }
    };

    ModelEditEvents editEvents;
    editEvents.manager = this;
    editEvents.containerId = evnt.container->getId();

    switch (editEvent.type) {
        case TileContainerEditEventType::ChangeFlags:
            break;
        case TileContainerEditEventType::ChangeTileID: {
            for (ui32 i = 0; i < editEvent.editCount; ++i) {
                TileContainerEditLayerEventData& edit = editEvent.changeLayerArray[i];

                if (edit.transformType != TileTransformationType::COUNT) {
                    assert(edit.prevId != TILE_ID_NONE && edit.newId != TILE_ID_NONE);
                    editEvents.transformEvents.emplace_back(ModelTransformEvent{ TileRepository::get().getTileModelID(edit.newId), edit.tileIndex, 0/*TODO: VARIANTS*/, edit.transformType});
                    continue;
                }

                if (edit.prevId != TILE_ID_NONE) {
                    if (TileRepository::get().getTileShape(edit.prevId) == TileShape::MODEL) {
                        editEvents.removeEvents.emplace_back(edit.tileIndex);
                    }
                }
                assert(edit.newId != edit.prevId);
                if (edit.newId != TILE_ID_NONE) {
                    const ModelID modelId = TileRepository::get().getTileModelID(edit.newId);
                    if (modelId != INVALID_MODEL_ID) {
                        f32 scale;
                        const ModelDef& modelDef = ModelRepository::get().getLoadedOrUnloadedAsset(modelId);
                        if (const FloraTileData* data = std::get_if<FloraTileData>(&edit.typeData)) {
                            scale = modelDef.getScaleFromFloraAge(data->age);
                        }
                        else {
                            scale = modelDef.getRandomScaleAtPosition(edit.worldPosition);
                        }
                        editEvents.addEvents.emplace_back(ModelAddEvent{ edit.worldPosition, edit.tileIndex, modelId, scale });
                    }
                }
            }
            break;
        }
        case TileContainerEditEventType::ChangeZPos: {
            for (ui32 i = 0; i < editEvent.editCount; ++i) {
                TileContainerEditZPosEventData& edit = editEvent.changeZPosArray[i];
                const Tile& tile = evnt.container->getTileAt(edit.tileIndex);
               
                if (edit.tileId != TILE_ID_NONE) {
                    const ModelID modelId = TileRepository::get().getTileModelID(edit.tileId);
                    if (modelId != INVALID_MODEL_ID) {
                        const ModelDef& modelDef = ModelRepository::get().getLoadedOrUnloadedAsset(modelId);
                        editEvents.heightAdjustEvents.emplace_back(ModelHeightAdjustEvent{ edit.worldPosition.z, edit.tileIndex });
                    }
                }
            }
            break;
        }
        case TileContainerEditEventType::ChangeOrientation:
            break;
        default:
            assert(false && "Unhandled model edit event in InstancedStaticModelRenderer");
            break;
    }
    static_assert(e_cast(TileContainerEditEventType::TERM) == BIT(4), "Update handler");


    if (editEvents.hasAny()) {

        ModelEditEvents* editPtr = new ModelEditEvents(std::move(editEvents));

        RenderThreadTasks::getInstance().addGenericTask([editPtr]() {
            TileContainerID containerId = editPtr->containerId;
            InstancedStaticModelManager* mgr = editPtr->manager;
            ModelRepository& modelRepo = ModelRepository::get();
            for (auto&& index : editPtr->removeEvents) {
                mgr->removeTileInstanceAtPosition(containerId, index);
            }
            for (auto&& addEvent : editPtr->addEvents) {
                // Assume no damage for now!
                mgr->addTileInstanceAtPosition(
                    containerId, addEvent.tileIndex, addEvent.modelId, addEvent.worldPosition, getTileModelRotationAtPosition(addEvent.worldPosition), 0 /*TODO: Variant*/, nullptr, addEvent.scale
                );
            }
            for (auto&& heightAdjustEvent : editPtr->heightAdjustEvents) {
                SpatialInstanceDataMap& tileContainerModels = mgr->mTileContainerTrackedModels[containerId];

                auto it = tileContainerModels.find(heightAdjustEvent.tileIndex);
                if (it != tileContainerModels.end()) {
                    TileModelInstanceIndex tileInstance = it->second;
                    // Just change zpos in the transform
                    mgr->mInstanceTransforms[tileInstance][3][2] = heightAdjustEvent.zPos;
                    mgr->onDirtyModelInstance(tileInstance);
                }
            }
            for (auto&& transformEvent : editPtr->transformEvents) {
                SpatialInstanceDataMap& tileContainerModels = mgr->mTileContainerTrackedModels[containerId];

                auto it = tileContainerModels.find(transformEvent.tileIndex);
                if (it != tileContainerModels.end()) {
                    TileModelInstanceIndex sourceInstance = it->second;
                    const ModelDef& nextModelDef = ModelRepository::get().getLoadedOrUnloadedAsset(transformEvent.nextId);
                    InstanceCrossfadeData& sourceCrossfadeData = mgr->mInstanceCrossfadeData[sourceInstance];
                    if (sourceCrossfadeData.isActive()) {
                        if (sourceCrossfadeData.mTransformationDataIndex != INVALID_TRANSFORMATION_DATA_INDEX) {
                            assert(sourceCrossfadeData.mTargetLOD == sourceCrossfadeData.mCurrentLOD);
                            // We transformed an already transforming tile. Just update the target and refresh the crossfade
                            TransformationData& transformationData = mgr->mTransformationData[sourceCrossfadeData.mTransformationDataIndex];
                            mgr->mInstanceDrawData[transformationData.toIndex] = InstanceDrawData(modelRepo.getDrawDataSpanKeyForModel(nextModelDef.getID()), nextModelDef.getID());
                            mgr->mInstanceGpuData[transformationData.toIndex].submeshDataIndex = mgr->mInstanceDrawData[transformationData.toIndex].key.startIndex;

                            sourceCrossfadeData.mCrossfade = -MATH_EPSILON;
                            InstanceCrossfadeData& targetCrossfadeData = mgr->mInstanceCrossfadeData[transformationData.toIndex];
                            assert(targetCrossfadeData.mTargetLOD == targetCrossfadeData.mCurrentLOD);
                            targetCrossfadeData.mCrossfade = MATH_EPSILON;
                        }
                        else {
                            // Force finish crossfade
                            sourceCrossfadeData.mCrossfade = 0.0f;
                            sourceCrossfadeData.mCurrentLOD = sourceCrossfadeData.mTargetLOD;
                        }
                    }
                    else {
                        // Remove old instance tracking as we will replace with new in addTileInstanceInternal
                        SpatialInstanceDataMap& tileContainerModels = mgr->mTileContainerTrackedModels[containerId];
                        tileContainerModels.erase(transformEvent.tileIndex);
                        mgr->mInstanceSources[sourceInstance] = std::monostate{};

                        // Create a new instance that is identical to our current one except for its model
                        TileModelInstanceIndex targetInstance = mgr->addTileInstanceInternal(
                            nextModelDef,
                            containerId,
                            transformEvent.tileIndex,
                            mgr->mInstanceTransforms[sourceInstance],
                            transformEvent.nextVariant,
                            nullptr /*no damage?*/,
                            mgr->mInstanceScales[sourceInstance]
                        );

                        sourceCrossfadeData.mCrossfade = -MATH_EPSILON;
                        sourceCrossfadeData.mTransformationDataIndex = mgr->mTransformationData.size();
                        mgr->mTransformationData.emplace_back(TransformationData{ sourceInstance, targetInstance, transformEvent.transformType, true });

                        InstanceCrossfadeData& targetCrossfadeData = mgr->mInstanceCrossfadeData[targetInstance];
                        targetCrossfadeData.mCrossfade = MATH_EPSILON;
                        targetCrossfadeData.mTransformationDataIndex = mgr->mTransformationData.size();
                        targetCrossfadeData.mCurrentLOD = sourceCrossfadeData.mCurrentLOD;
                        targetCrossfadeData.mTargetLOD = sourceCrossfadeData.mTargetLOD;
                        mgr->mTransformationData.emplace_back(TransformationData{ sourceInstance, targetInstance, transformEvent.transformType, false });

                        mgr->mNumActiveTransformationTransitions += 2;
                    }
                }
            }
            delete editPtr;
        });
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

    RenderThreadTasks::getInstance().addGenericTask([taskData]() {
        TileDamagedEvent evnt = taskData->damageEvent;

        f32v2 hitNormal = evnt.impactNormal;
        hitNormal = MathUtil::rotateVector2D(hitNormal, 90.0f);

        const LiteTileHandle handle(taskData->container->getId(), evnt.tileIndex);
        if (taskData->modelManager->playAnimationOnInstanceAtPosition(handle, StaticModelAnimationTypes::HitWiggle, hitNormal, TileRepository::get().getLoadedOrUnloadedAsset(evnt.tileId).modelRef.getAssetID())) {
            // Apply damage to model
            taskData->modelManager->onTileInstanceDamageChanged(taskData->container->getId(), evnt.tileIndex, evnt.damageData);
        }

        taskData->container->decRef();
        delete taskData;
    });
}

StaticModelInstanceID InstancedStaticModelManager::addLooseModelInstance(ModelID modelId, const glm::quat& orient, f32v3 position, ui8 variantIndex, f32 scale) {
    const StaticModelInstanceID id = ++mNextLooseInstanceID;

    PendingLooseModelInstance pendingInstance{
        .orient = orient,
        .position = position,
        .modelId = modelId,
        .instanceId = id,
        .variantIndex = variantIndex,
        .type = PendingLooseModelInstance::Type::Add,
        .scale = scale
    };

    mPendingLooseModelInstances.enqueue(pendingInstance);

    return id;

}

void InstancedStaticModelManager::removeLooseModelInstance(ModelID modelId, StaticModelInstanceID instanceId) {
    PendingLooseModelInstance pendingInstance{
        .modelId = modelId,
        .instanceId = instanceId,
        .type = PendingLooseModelInstance::Type::Remove
    };

    mPendingLooseModelInstances.enqueue(pendingInstance);
}

void InstancedStaticModelManager::changeLooseModelInstanceScale(ModelID modelId, StaticModelInstanceID instanceId, const glm::quat& orient, f32v3 position, f32 scale) {
    PendingLooseModelInstance pendingInstance{
      .orient = orient,
      .position = position,
      .modelId = modelId,
      .instanceId = instanceId,
      .type = PendingLooseModelInstance::Type::ChangeTransform,
      .scale = scale
    };

    mPendingLooseModelInstances.enqueue(pendingInstance);
}

void InstancedStaticModelManager::init() {
    mNeedsInit = false;

    // Water not supported
    for (MaterialRenderPassType type : CROSSFADE_PASSES) {
        mCrossfadeBuffers[(int)type] = std::make_unique<GpuStreamingDataBuffer>(MAX_LOD_DITHER_TRANSITION_DRAWS, sizeof(f32));
        mDrawCommandsCrossfade[(int)type] = std::make_unique<GLDrawCommandBuffer>(MAX_LOD_DITHER_TRANSITION_DRAWS);
        mDrawCommandsTransformations[(int)type] = std::make_unique<GLDrawCommandBuffer>(MAX_TRANSFORMATION_TRANSITION_DRAWS);
    }

}

void InstancedStaticModelManager::updatePendingLooseModelInstances() {
    ASSERT_RENDER_THREAD();
    PROFILE_FUNCTION();
    constexpr i32 MAX_DEQUEUE = 1024;
    PendingLooseModelInstance instances[MAX_DEQUEUE];
    if (size_t count = mPendingLooseModelInstances.try_dequeue_bulk(instances, MAX_DEQUEUE)) {
        for (size_t i = 0; i < count; ++i) {
            PendingLooseModelInstance& instance = instances[i];
            switch (instance.type) {
                case PendingLooseModelInstance::Type::Add: {
                    // TODO: Construct transform in place so no copy?
                    const f32m4 transform = MathUtil::createTransformMatrix(instance.position, instance.orient, instance.scale);
                    addLooseInstanceInternal(instance.modelId, instance.instanceId, transform, instance.variantIndex, instance.scale);
                    break;
                }
                case PendingLooseModelInstance::Type::Remove:
                    removeLooseInstanceInternal(instance.instanceId);
                    break;
                case PendingLooseModelInstance::Type::ChangeTransform:
                    updateLooseInstanceTransformInternal(instance.instanceId, MathUtil::createTransformMatrix(instance.position, instance.orient, instance.scale), instance.scale);
                    break;
                default:
                    assert(false);
            }
            static_assert(e_count(PendingLooseModelInstance::Type) == 3);
        }
    }
}

void InstancedStaticModelManager::removeModelInstanceInternal(TileModelInstanceIndex instanceIndex) {
    
    TileModelInstanceIndex fromTransformationToRemove = INVALID_TILE_MODEL_INSTANCE_INDEX;

    onDirtyModelInstance(instanceIndex);

    ModelInstanceOwnerVariant backSource = mInstanceSources.back();
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
        backRef->second = instanceIndex;
    }
    else if (std::holds_alternative<StaticModelInstanceID>(backSource)) {
        // Loose model
        const StaticModelInstanceID backInstanceID = std::get<StaticModelInstanceID>(backSource);
        auto&& backRef = mLooseStaticModelInstances.find(backInstanceID);
        assert(backRef != mLooseStaticModelInstances.end());
        backRef->second = instanceIndex;
    }
    // Else, back is untracked (std::monostate) so do nothing

    mInstanceSources[instanceIndex] = backSource;
    mInstanceSources.pop_back();

    mInstanceScales[instanceIndex] = mInstanceScales.back();
    mInstanceScales.pop_back();

    InstanceCrossfadeData& crossfadeData = mInstanceCrossfadeData[instanceIndex];
    if (crossfadeData.isActive()) {
        // If we are crossfading as a transformation, make sure to remove the other instance
        if (crossfadeData.mTransformationDataIndex != INVALID_TRANSFORMATION_DATA_INDEX) [[unlikely]] {
            TransformationData& transformationData = mTransformationData[crossfadeData.mTransformationDataIndex];
            // The "to" index is the authoritative index, so we remove the "from" index
            if (transformationData.isFrom == false) {
                fromTransformationToRemove = transformationData.fromIndex;
            }
            assert(mNumActiveTransformationTransitions);
            --mNumActiveTransformationTransitions;
        }
        else {
            assert(mNumActiveLodTransitions);
            --mNumActiveLodTransitions;
        }
    }

    // If the back is transforming
    if (mInstanceCrossfadeData.back().mTransformationDataIndex != INVALID_TRANSFORMATION_DATA_INDEX) {
        InstanceCrossfadeData& backCrossfadeData = mInstanceCrossfadeData.back();
        const TileModelInstanceIndex backIndex = mInstanceCrossfadeData.size() - 1;
        assert(backCrossfadeData.isActive());
        TransformationData& backTransformationData = mTransformationData[backCrossfadeData.mTransformationDataIndex];
        // (and it isn't the one we just deleted), we need to update the indices pointing to it
        if (backTransformationData.fromIndex != instanceIndex && backTransformationData.toIndex != instanceIndex) {
            // One of these will be the same as backTransformationData
            const TransformationDataIndex fromDataIndex = mInstanceCrossfadeData[backTransformationData.fromIndex].mTransformationDataIndex;
            const TransformationDataIndex toDataIndex = mInstanceCrossfadeData[backTransformationData.toIndex].mTransformationDataIndex;
            if (backTransformationData.fromIndex == backIndex) {
                mTransformationData[fromDataIndex].fromIndex = instanceIndex;
                mTransformationData[toDataIndex].fromIndex = instanceIndex;
            }
            else {
                assert(backTransformationData.toIndex == backIndex);
                mTransformationData[fromDataIndex].toIndex = instanceIndex;
                mTransformationData[toDataIndex].toIndex = instanceIndex;
            }
        }
    }

    crossfadeData = mInstanceCrossfadeData.back();
    mInstanceCrossfadeData.pop_back();

    // Replace this instance with back instance
    mInstanceTransforms[instanceIndex] = std::move(mInstanceTransforms.back());
    mInstanceTransforms.pop_back();
    const ui32 damageModelIndex = mInstanceGpuData[instanceIndex].damageModelIndex;
    // If we had a damage model, need to remove it and fixup ref
    if (damageModelIndex != 0) {
        removeDamageModelInternal(damageModelIndex);
    }
    mInstanceGpuData[instanceIndex] = std::move(mInstanceGpuData.back());
    mInstanceGpuData.pop_back();

    decrementDrawCommandsCount(mInstanceDrawData[instanceIndex].key);
    mInstanceDrawData[instanceIndex] = mInstanceDrawData.back();
    mInstanceDrawData.pop_back();

    // If we removed an in progress transforming tile, we must also remove the source
    if (fromTransformationToRemove != INVALID_TILE_MODEL_INSTANCE_INDEX) [[unlikely]] {
        removeModelInstanceInternal(fromTransformationToRemove);
    }
}

TileModelInstanceIndex InstancedStaticModelManager::addTileInstanceInternal(
    const ModelDef& modelDef, TileContainerID containerId, TileIndex tileIndex, const f32m4& transform, ui8 variantIndex, TileDamageDataPtr damageData, f32 scale
) {

    const size_t instanceIndex = mInstanceTransforms.size();
    onDirtyModelInstance(instanceIndex);

    // Store per tile references (note scale is already applied)
    mInstanceTransforms.emplace_back(transform);
    InstanceGpuData& newGpuData = mInstanceGpuData.emplace_back();

    ModelRepository& modelRepo = ModelRepository::get();
    const VariantIndexData variantData = modelRepo.getVariantArrayIndexDataForModel(modelDef.getID());

    mInstanceScales.emplace_back(scale);
    mInstanceCrossfadeData.emplace_back();

    newGpuData.variantIndex = variantData.offset + (InstanceVariantIndexType)variantIndex * variantData.stride;
    if (damageData) {
        newGpuData.damageModelIndex = mModelDamageZonesGpuData.size();
        ModelDamageZoneGpuData& gpuDamageData = mModelDamageZonesGpuData.emplace_back();
        gpuDamageData.damageZones = damageData->getShellDamageZones();
    }
    else {
        newGpuData.damageModelIndex = 0;
    }
    mInstanceSources.emplace_back(ModelInstanceContainerOwner{ containerId, tileIndex });

    SpatialInstanceDataMap& tileContainerModels = mTileContainerTrackedModels[containerId];
    assert(tileContainerModels.find(tileIndex) == tileContainerModels.end());
    tileContainerModels.emplace(tileIndex, instanceIndex);

    mInstanceDrawData.emplace_back(modelRepo.getDrawDataSpanKeyForModel(modelDef.getID()), modelDef.getID());
    newGpuData.submeshDataIndex = mInstanceDrawData.back().key.startIndex;
    incrementDrawCommandsCount(mInstanceDrawData.back().key);
    return instanceIndex;
}

void InstancedStaticModelManager::onTileInstanceDamageChanged(TileContainerID containerId, TileIndex tileIndex, const TileDamageData& damageData) {

    TileModelInstanceIndex instance = getTileInstanceIndexAtPosition(LiteTileHandle{ containerId, tileIndex });
    assert(instance != INVALID_TILE_MODEL_INSTANCE_INDEX);

    const bool isUndamaged = damageData.getShellDamageZones() == TileDamageData().getShellDamageZones();

    ui32& damageModelIndex = mInstanceGpuData[instance].damageModelIndex;
    if (damageModelIndex != 0) {
        if (isUndamaged) {
            removeDamageModelInternal(damageModelIndex);
            damageModelIndex = 0;
        }
        else {
            ModelDamageZoneGpuData& gpuDamageData = mModelDamageZonesGpuData[damageModelIndex];
            gpuDamageData.damageZones = damageData.getShellDamageZones();
            // Immediately update buffer
            glNamedBufferSubData(
                mDamageZonesSSBO,
                damageModelIndex * sizeof(ModelDamageZoneGpuData),
                sizeof(ModelDamageZoneGpuData),
                &gpuDamageData
            );
        }
    }
    else {
        if (isUndamaged) {
            // We are already tracking the damage model 0, do nothing
            return;
        }
        damageModelIndex = mModelDamageZonesGpuData.size();
        ModelDamageZoneGpuData& gpuDamageData = mModelDamageZonesGpuData.emplace_back();
        gpuDamageData.damageZones = damageData.getShellDamageZones();
        // Immediately rebuild SSBO
        GL.glDeleteBuffers(1, &mDamageZonesSSBO);
        GL.glCreateBuffers(1, &mDamageZonesSSBO);
        GL.glNamedBufferStorage(mDamageZonesSSBO, sizeof(ModelDamageZoneGpuData) * mModelDamageZonesGpuData.size(), nullptr, GL_DYNAMIC_STORAGE_BIT);
        GL.glNamedBufferSubData(mDamageZonesSSBO, 0, sizeof(ModelDamageZoneGpuData) * mModelDamageZonesGpuData.size(), mModelDamageZonesGpuData.data());
    }
    // Update damage index buffer
    glNamedBufferSubData(
        mInstanceDataVbo,
        instance * sizeof(InstanceGpuData) + offsetof(InstanceGpuData, damageModelIndex),
        sizeof(ui32),
        &damageModelIndex
    );
}

void InstancedStaticModelManager::removeDamageModelInternal(ui32 damageModelIndex) {
    mModelDamageZonesGpuData[damageModelIndex] = std::move(mModelDamageZonesGpuData.back());
    mModelDamageZonesGpuData.pop_back();
    // Fixup relocated
    // TODO: This is inefficient
    for (size_t i = 0; i < mInstanceGpuData.size(); ++i) {
        InstanceGpuData& data  = mInstanceGpuData[i];
        if (data.damageModelIndex > damageModelIndex) {
            --data.damageModelIndex;
            glNamedBufferSubData(
                mInstanceDataVbo,
                i * sizeof(InstanceGpuData) + offsetof(InstanceGpuData, damageModelIndex),
                sizeof(ui32),
                &data.damageModelIndex
            );
        }
    }
}

void InstancedStaticModelManager::addLooseInstanceInternal(
    ModelID modelId, StaticModelInstanceID instanceId, const f32m4& transform, ui8 variantIndex, f32 scale
) {
    const VariantIndexData variantData = ModelRepository::get().getVariantArrayIndexDataForModel(modelId);
    increfModelDef(modelId, 1);
    const size_t instanceIndex = mInstanceTransforms.size();
    onDirtyModelInstance(instanceIndex);

    mInstanceTransforms.emplace_back(transform);
    InstanceGpuData& newGpuData = mInstanceGpuData.emplace_back();
    newGpuData.variantIndex = variantData.offset + (InstanceVariantIndexType)variantIndex * variantData.stride;
    newGpuData.damageModelIndex = 0; // Currently loose models do not support damage
    mInstanceSources.emplace_back(instanceId);

    mInstanceScales.emplace_back(scale);
    mInstanceCrossfadeData.emplace_back();

    // Store instance lookup
    mLooseStaticModelInstances.emplace(std::make_pair(instanceId, (ui32)instanceIndex));

    mInstanceDrawData.emplace_back(ModelRepository::get().getDrawDataSpanKeyForModel(modelId), modelId);
    newGpuData.submeshDataIndex = mInstanceDrawData.back().key.startIndex;
    incrementDrawCommandsCount(mInstanceDrawData.back().key);
}

void InstancedStaticModelManager::removeLooseInstanceInternal(StaticModelInstanceID instanceId) {

    auto&& lit = mLooseStaticModelInstances.find(instanceId);
    assert(lit != mLooseStaticModelInstances.end());
    const ui32 instanceIndex = lit->second;

    removeModelInstanceInternal(instanceIndex);

    mLooseStaticModelInstances.erase(lit);

}

void InstancedStaticModelManager::updateLooseInstanceTransformInternal(StaticModelInstanceID instanceId, const f32m4 transform, f32 scale) {

    auto&& lit = mLooseStaticModelInstances.find(instanceId);
    assert(lit != mLooseStaticModelInstances.end());
    const ui32 instanceIndex = lit->second;

    onDirtyModelInstance(instanceIndex);

    mInstanceTransforms[instanceIndex] = transform;
    mInstanceScales[scale] = scale;
}

void InstancedStaticModelManager::updateAnimatedModels(f32 elapsedSec) {

    for (auto it = mAnimatedTileInstances.begin(); it != mAnimatedTileInstances.end();) {

        StaticMeshAnimation& animation = it->second;
        const f32 animDuration = STATIC_MODEL_ANIM_DURATIONS_SEC[e_cast(animation.animType)];
        animation.currentTimeSec += elapsedSec;
        if (animation.currentTimeSec >= animDuration) {
            it = mAnimatedTileInstances.erase(it);
        }
        else {
            TileModelInstanceIndex instance = getTileInstanceIndexAtPosition(it->first);
            if (instance == INVALID_TILE_MODEL_INSTANCE_INDEX) {
                it = mAnimatedTileInstances.erase(it);
                continue;
            }
            const f32m4& baseTransform = mInstanceTransforms[instance];

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
            glNamedBufferSubData(
                mTransformsVbo,
                instance * sizeof(f32m4),
                sizeof(f32m4),
                &newTransform[0][0]
            );

            ++it;
        }
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

void InstancedStaticModelManager::incrementDrawCommandsCount(ModelBatchSubmeshDrawDataSpanKey drawDataKey) {
    const ModelBatchSubmeshDrawData* drawData = ModelRepository::get().getSubmeshDrawDataArrayForModel(drawDataKey);
    for (ui32 i = 0; i < drawDataKey.count; ++i) {
        const ModelBatchSubmeshDrawData& submeshDrawData = drawData[i];
        ++mDrawCommandsCount[e_cast(submeshDrawData.renderPass)];
        if (submeshDrawData.castsShadow) {
            ++mDrawCommandsShadowsCount[e_cast(submeshDrawData.renderPass)];
        }
    }
}

void InstancedStaticModelManager::decrementDrawCommandsCount(ModelBatchSubmeshDrawDataSpanKey drawDataKey) {
    const ModelBatchSubmeshDrawData* drawData = ModelRepository::get().getSubmeshDrawDataArrayForModel(drawDataKey);
    for (ui32 i = 0; i < drawDataKey.count; ++i) {
        const ModelBatchSubmeshDrawData& submeshDrawData = drawData[i];
        assert(mDrawCommandsCount[e_cast(submeshDrawData.renderPass)]);
        --mDrawCommandsCount[e_cast(submeshDrawData.renderPass)];
        if (submeshDrawData.castsShadow) {
            assert(mDrawCommandsShadowsCount[e_cast(submeshDrawData.renderPass)]);
            --mDrawCommandsShadowsCount[e_cast(submeshDrawData.renderPass)];
        }
    }
}

void InstancedStaticModelManager::onDirtyModelInstance(TileModelInstanceIndex instanceIndex) {
    if (instanceIndex < mFirstDirtyInstance) {
        mFirstDirtyInstance = instanceIndex;
    }
    if (instanceIndex > mLastDirtyInstance) {
        mLastDirtyInstance = instanceIndex;
    }
}
