#include "stdafx.h"
#include "CharacterRenderer.h"

#include "ecs/component/PhysicsComponent.h"
#include "ecs/component/CharacterControlComponent.h"

#include "resources/IAssetRepository.h"
#include "rendering/MaterialShaderDef.h"
#include "debugging/DebugRenderer.h"

#include "rendering/MaterialShaderRepository.h"

#include "rendering/MaterialRenderer.h"
#include "rendering/TileVertex.h"
#include "rendering/renderstate/CharacterRenderState.h"
#include "rendering/mesh/MeshDrawer.h"
#include "rendering/animation/AnimMachineInstance.h"
#include "rendering/model/skeletal/SkeletalAnimator.h"

#include "resources/ModelRepository.h"
#include "resources/AnimationRepository.h"
#include "resources/ResourceManager.h"

#include "world/World.h"
#include "ecs/IFullECS.h"

#include "options/DebugOptions.h"

#include "math/Random.h"

#include "camera/Camera3D.h"

#include "definitions/RigDef.h"
#include "ozz/base/containers/vector.h"
#include "ozz/base/maths/simd_math.h"

// TODO: Utility?
void decomposeMatrix(const f64m4& m, f64v3& pos, f64q& rot) {
    pos = m[3];
    rot = glm::quat_cast(m);
}

// How many extra spots for transforms are allocated in the GPU streaming buffer
constexpr i32 TRANSFORMS_PADDING_SIZE = 4;

struct CharacterRendererCharacterState {
    AnimMachineInstance animInstance;
    std::vector<LinkedSubmodelData> linkedSubmodels;
    ModularHumanoidCharacterModel modularModel;
    i32 boneTransformCount = 0; // Based on submesh transform count, varies based on skinning
    i16 submeshPartCount = 0;
};

CharacterRenderer::CharacterRenderer() :
    mShaderHandle(MaterialShaderRepository::get().getAssetHandle(CStrToken("character"))) {
    mConsumerToken = std::make_unique<moodycamel::ConsumerToken>(mModelsToUpdate);
}

CharacterRenderer::~CharacterRenderer() {
    if (mRegistry) {
        mRegistry->on_construct<CharacterModelComponent>().disconnect<&CharacterRenderer::onCharacterModelConstruct>(this);
        mRegistry->on_destroy<CharacterModelComponent>().disconnect<&CharacterRenderer::onCharacterModelDestroy>(this);
        mCharacterModelListeners.reset();
    }
}

void CharacterRenderer::onWorldBegin(World& world) {
    ASSERT_GAME_THREAD();
    // Only register once
    if (!mRegistry) {
        mRegistry = &world.getECS().mRegistry;
        mRegistry->on_construct<CharacterModelComponent>().connect<&CharacterRenderer::onCharacterModelConstruct>(this);
        mRegistry->on_destroy<CharacterModelComponent>().connect<&CharacterRenderer::onCharacterModelDestroy>(this);
        CharacterModelEvents::registerCharacterModelListeners(mCharacterModelListeners);
        CharacterModelEvents::addSubmodelAddedListener(mCharacterModelListeners, [this](const CharacterModelEvent& event) {
            onCharacterModelSubmodelAdded(event.owner, event.submodel);
        });
        CharacterModelEvents::addSubmodelRemovedListener(mCharacterModelListeners, [this](const CharacterModelEvent& event) {
            onCharacterModelSubmodelRemoved(event.owner, event.submodel);
        });
    }
}

void CharacterRenderer::frameBegin() {
    ASSERT_RENDER_THREAD();

    constexpr ui32 BULK_DEQUEUE_SIZE = 16;
    CharacterModelUpdateData modelsToUpdate[BULK_DEQUEUE_SIZE];

    if (const size_t count = mModelsToUpdate.try_dequeue_bulk(*mConsumerToken, modelsToUpdate, BULK_DEQUEUE_SIZE)) {
        for (size_t i = 0; i < count; ++i) {
            CharacterModelUpdateData& updateData = modelsToUpdate[i];
            switch (updateData.type) {
                case CharacterModelUpdateType::Add:
                    addCharacterModelInternal(updateData.entityId, updateData.model, std::move(updateData.submodels));
                    break;
                case CharacterModelUpdateType::Remove:
                    removeCharacterModelInternal(updateData.entityId);
                    break;
                case CharacterModelUpdateType::AddSubmodel:
                    assert(updateData.submodels.size() == 1);
                    addSubmodelInternal(updateData.entityId, updateData.submodels[0]);
                    break;
                case CharacterModelUpdateType::RemoveSubmodel:
                    assert(updateData.submodels.size() == 1);
                    removeSubmodelInternal(updateData.entityId, updateData.submodels[0]);
                    break;
                default:
                    assert(false);
                    break;

            }
            static_assert(e_count(CharacterModelUpdateType) == 4);
        }
    }
}

void CharacterRenderer::addCharacterModel(entt::entity entityId, ModularHumanoidCharacterModel modularCharacter, std::vector<LinkedSubmodelData> submodels) {
    if (IS_RENDER_THREAD()) {
        addCharacterModelInternal(entityId, modularCharacter, std::move(submodels));
    }
    else {
        mModelsToUpdate.enqueue({ entityId, modularCharacter, std::move(submodels) });
    }
}

void CharacterRenderer::removeCharacterModel(entt::entity entityId) {
    if (IS_RENDER_THREAD()) {
        removeCharacterModelInternal(entityId);
    }
    else {
        mModelsToUpdate.enqueue({ entityId, CharacterModelUpdateType::Remove });
    }
}

void CharacterRenderer::playOneShotAnimation(entt::entity entityId, AssetID animationId) {

    ModelRepository& modelRepo = ModelRepository::get();
    auto it = mCharacterModels.find(entityId);
    if (it != mCharacterModels.end()) {
        // TODO: Allow lazy load anim + catchup?
        const AnimationDef* animDef = AnimationRepository::get().tryGetLoadedAsset(animationId);
        if (!animDef) panic("Tried to play one shot anim {} that was not loaded", animationId);
        it->second.animInstance.tryPlayOneShot(*animDef);
    }
}

void CharacterRenderer::renderCharactersAndGatherSubmodels(const Camera3D& camera, const std::vector<CharacterRenderState>& characters, f32 elapsedSec, f32 frameAlpha, DynamicModelInstanceStateContainer& outLinkedSubmodels) {
    UNUSED(frameAlpha);
    PROFILE_FUNCTION();


    const MaterialShaderDef* shaderDef = mShaderHandle->tryGetLoadedAsset();
    if (!shaderDef) return;

    if (!mTotalSubmeshParts) [[unlikely]] return;

    ModelRepository& modelRepo = ModelRepository::get();
    vcore::ThreadPool& threadPool = Services::Threadpool::ref();

    static_assert(sizeof(f32m4) == sizeof(ozz::math::Float4x4));
    GpuStreamingDataBuffer::reallocateFuzzedIfNeeded(mBoneTransformsBuffer, mTotalJointTransforms, sizeof(f32m4), 256);
    GpuStreamingDataBuffer::reallocateFuzzedIfNeeded(mModelTransformsBuffer, characters.size(), sizeof(f32m4), 64);
    GpuStreamingDataBuffer::reallocateFuzzedIfNeeded(mSubmeshDataBuffer, mTotalSubmeshParts, sizeof(SubmeshInstanceData), 128);
    // TODO: Other shader passes
    GLDrawCommandBuffer::reallocateFuzzedIfNeeded(mDrawCommands, mTotalSubmeshParts, 128);

    ozz::math::Float4x4* boneTransformsArray = static_cast<ozz::math::Float4x4*>(mBoneTransformsBuffer->frameBeginAndGetDataForUpdate());
    f32m4* modelTransformsArray = static_cast<f32m4*>(mModelTransformsBuffer->frameBeginAndGetDataForUpdate());
    SubmeshInstanceData* submeshDataArray = static_cast<SubmeshInstanceData*>(mSubmeshDataBuffer->frameBeginAndGetDataForUpdate());
    mDrawCommands->frameBegin();

    DrawElementsIndirectCommand* drawCommandsBuffer = mDrawCommands->getDrawCommands().data();

    const ui32 boneTransformOffset = mBoneTransformsBuffer->getCurrentElementOffset();
    const ui32 modelTransformOffset = mModelTransformsBuffer->getCurrentElementOffset();

    ui32 TMP_TOTAL_JOB_COUNT = 0;
    std::atomic_int jobCount = 0;
    ui32 numDrawCommands = 0;
    ui32 boneTransformsIndex = 0;
    ui32 modelTransformsIndex = 0;
    ui32 submeshDataIndex = 0;
    for (const CharacterRenderState& frameState : characters) {
        auto it = mCharacterModels.find(frameState.mEntityID);
        if (it == mCharacterModels.end()) {
            continue;
        }

        CharacterRendererCharacterState& characterState = it->second;
        const ModelLodParams& lodParams = modelRepo.getLodParams(characterState.modularModel.baseModel);
        const bool isVisible = camera.sphereIsVisible(frameState.mPos, lodParams.boundingSphereRadius);

        ++jobCount;
        ++TMP_TOTAL_JOB_COUNT;
        threadPool.addTask(
            [&camera, &frameState, &characterState, &lodParams, elapsedSec, boneTransformsArray, modelTransformsArray,
            submeshDataArray, drawCommandsBuffer, isVisible, &jobCount, numDrawCommands, boneTransformsIndex, modelTransformsIndex,
            submeshDataIndex, boneTransformOffset, modelTransformOffset, &outLinkedSubmodels, this]() mutable
        {

            ModelRepository& modelRepo = ModelRepository::get();
            const ModularHumanoidCharacterModel& modularCharacter = characterState.modularModel;
            const RigDef& rig = characterState.animInstance.getRig();

            const f32v3& position = frameState.mPos;

            // TODO: combine with CharacterRenderState?
            AnimVariables variables;
            variables.locomotionMode = frameState.mLocomotionMode;
            variables.velocity2d = frameState.mVelocity2D;
            variables.speed = glm::length(frameState.mVelocity2D);
            ozz::math::Float4x4 modelsBuffer[MAX_JOINTS_IN_RIG];
            OzzMatrixSpan modelMatrices(modelsBuffer, rig.mSkeleton.num_joints());

            // Update animation TODO: Multithreaded?
            // When not visible we pass null output buffer, signaling that we dont want to update bones, only tick the anim states
            characterState.animInstance.update(elapsedSec, variables, isVisible ? modelMatrices : OzzMatrixSpan(nullptr, (size_t)0));

            if (isVisible) {

                // Manually set world translation (TODO: Can set this on transform initialize for less instructions)
                const f32v3 offset = position - camera.getPosition();

                const f32 angle = frameState.mRotation;

                const f32 angleZ = DEG_TO_RAD(90.0f) + angle;
                const f32 angleX = DEG_TO_RAD(90.0f);

                const f32 cosZ = cosf(angleZ);
                const f32 sinZ = sinf(angleZ);
                const f32 cosX = cosf(angleX);
                const f32 sinX = sinf(angleX);
                const f32 oneMinusCosX = 1.f - cosX;

                // Construct the combined transformation matrix
                // Hand optimized form of this:
                //transform = glm::rotate(transform, angleZ, f32v3(0.0f, 0.0f, 1.0f));
                //transform = glm::rotate(transform, angleX, f32v3(1.0f, 0.0f, 0.0f));
                const f32 tmp = cosX + oneMinusCosX;
                modelTransformsArray[modelTransformsIndex] = f32m4(
                    cosZ * tmp, sinZ * tmp, 0.0f, 0.0f,
                    -sinZ * cosX, cosZ * cosX, sinX, 0.0f,
                    sinZ * sinX, -cosZ * sinX, cosX, 0.0f,
                    offset.x, offset.y, offset.z, 1.0f
                );
                const f32m4& cameraRelTransform = modelTransformsArray[modelTransformsIndex];

                const f32 distSQ = glm::length2(offset);
                MeshLODLevel lod = lodParams.selectLOD(distSQ);

                // TODO: We should batch render these
                for (SubmeshID partId : modularCharacter.partIds) {
                    if (partId == INVALID_SUBMESH_ID) [[unlikely]] {
                        continue;
                    }

                    SubmeshInstanceData& instanceData = submeshDataArray[submeshDataIndex];
                    instanceData.boneTransformStartIndex = boneTransformsIndex + boneTransformOffset;
                    instanceData.modelTransformIndex = modelTransformsIndex + modelTransformOffset;
                    instanceData.variantIndex = modelRepo.getSubmeshIndexDataOffset(partId);

                    const MeshSkeletonData& skeletonData = *modelRepo.getSubmeshSkeletonData(partId);
                    const ModelBatchSubmeshDrawData& drawData = modelRepo.getSubmeshDrawData(partId);
                    // Skin animation to mesh
                    OzzMatrixSpan skinningMatrices(&boneTransformsArray[boneTransformsIndex], skeletonData.mNumJoints);
                    if (!SkeletalAnimator::skinModelMatricesToMesh(ozz::make_span(modelMatrices), skeletonData, skinningMatrices)) {
                        panic("Anim skinning fail!");
                    }
                    boneTransformsIndex += skeletonData.mNumJoints;

                    // TODO: REMOVE WHEN ERRA HAS ANIMS
                    for (size_t j = 0; j < skeletonData.mNumJoints; ++j) {
                        skinningMatrices[j] = ozz::math::Float4x4::identity();
                    }

                    // TODO: Support others
                    assert(drawData.batchID == modelRepo.getModelBatch(
                        ModelBatchKey{ MeshIndexType::USHORT, VertexType::SKINNED_MODEL,MaterialRenderPassType::Default }).getId()
                    );

                    // Build draw command
                    const MeshLODDrawInfo& drawInfo = drawData.lodDrawInfo[e_cast(lod)];
                    DrawElementsIndirectCommand& cmd = drawCommandsBuffer[numDrawCommands++];
                    cmd.baseInstance_ = submeshDataIndex;
                    cmd.instanceCount_ = 1;
                    cmd.baseVertex_ = drawData.baseVertex;
                    cmd.firstIndex_ = drawInfo.startIndex;
                    cmd.count_ = drawInfo.indexCount;

                    ++submeshDataIndex;
                }

                // Submodels

                for (LinkedSubmodelData& submodel : characterState.linkedSubmodels) {
                    AssetHandlePtr<ModelDef> submodelHandle = ModelRepository::get().getAssetHandle(submodel.modelId);
                    if (const ModelDef* def = submodelHandle->tryGetLoadedAsset()) {

                        const f32m4 boneTransform = std::bit_cast<f32m4>(modelMatrices[submodel.attachBoneIndex]);
                        const f64m4 worldTransform(
                            cameraRelTransform[0][0], cameraRelTransform[0][1], 0.0f, 0.0f,
                            cameraRelTransform[1][0], cameraRelTransform[1][1], cameraRelTransform[1][2], 0.0f,
                            cameraRelTransform[2][0], cameraRelTransform[2][1], cameraRelTransform[2][2], 0.0f,
                            position.x, position.y, position.z, 1.0f
                        );
                        f64q rotation;
                        f64v3 worldPosition;
                        decomposeMatrix(worldTransform * f64m4(boneTransform), worldPosition, rotation);
                        {
                            std::lock_guard lock(mOutLinkedSubmodelsMutex);
                            outLinkedSubmodels.add(rotation, worldPosition, submodel.modelId);
                        }
                    }
                }
            }
            --jobCount;
        }, TaskPriority::High);

        if (isVisible) {
            ++modelTransformsIndex;
            submeshDataIndex += characterState.submeshPartCount;
            numDrawCommands += characterState.submeshPartCount;
            boneTransformsIndex += characterState.boneTransformCount;
        }
    }

    // Help finish tasks
    ui32 TMP_MAIN_TASK_COUNT = 0;
    while (jobCount > 0) {
        if (!threadPool.tryProcessHighPriorityTask()) {
            // Probably waiting on another thread to finish
            std::this_thread::yield();
        }
        else {
            ++TMP_MAIN_TASK_COUNT;
        }
    }

    if (!numDrawCommands) {
        return;
    }

    mBoneTransformsBuffer->flushDataAndIncrementFrame(boneTransformsIndex);
    mModelTransformsBuffer->flushDataAndIncrementFrame(modelTransformsIndex);
    mSubmeshDataBuffer->flushDataAndIncrementFrame(submeshDataIndex);

    mDrawCommands->setNumActiveCommands(numDrawCommands);
    mDrawCommands->uploadDrawCommands();

    MaterialRenderer::bindMaterialShaderForRender(*shaderDef);

    const ModelBatch& modelBatch = modelRepo.getModelBatch(ModelBatchKey{ MeshIndexType::USHORT, VertexType::SKINNED_MODEL,MaterialRenderPassType::Default });
    // Variant data
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, BUFFER_BASE_MODEL_VARIANT_DATA_SSBO, modelRepo.getModelVariantDataSSBO());
    mBoneTransformsBuffer->bindBufferAsSSBO(BUFFER_BASE_SKINNING_MATRICES);
    mModelTransformsBuffer->bindBufferAsSSBO(BUFFER_BASE_MODEL_TRANSFORMS_SSBO);

    VGBuffer vao = modelBatch.getVao();
    mSubmeshDataBuffer->bindAsVertexArrayVertexBuffer(vao, MODEL_INSTANCE_DATA_BINDING_POINT, 0, sizeof(SubmeshInstanceData));
    static_assert(sizeof(SubmeshInstanceData) == sizeof(ui32v3));
    
    modelBatch.bindSkeletalModelAttribs();

    glBindVertexArray(vao);
    assert(modelBatch.getIndexType() == MeshIndexType::USHORT);
    mDrawCommands->multiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_SHORT);

    // TODO: Material specific
    glEnable(GL_CULL_FACE);
    checkGlError("InstancedStaticModelRenderer::renderModelPass");

}

CharacterRendererCharacterState* CharacterRenderer::tryGetCharacterRenderStateForDebug(entt::entity entityId) {
    ASSERT_RENDER_THREAD();
    auto it = mCharacterModels.find(entityId);
    if (it != mCharacterModels.end()) {
        return &it->second;
    }
    return nullptr;
}

void CharacterRenderer::addCharacterModelInternal(
    entt::entity entityId,
    const ModularHumanoidCharacterModel& modularCharacter,
    std::vector<LinkedSubmodelData>&& submodels
) {
    ASSERT_RENDER_THREAD();
    ModelRepository& modelRepo = ModelRepository::get();
    auto it = mCharacterModels.find(entityId);
    // Rare case where we reuse the entity ID
    if (it != mCharacterModels.end()) [[unlikely]] {
        mCharacterModels.erase(it);
    }
    CharacterRendererCharacterState& renderState = mCharacterModels.emplace(entityId, CharacterRendererCharacterState()).first->second;
    renderState.modularModel = modularCharacter;
    renderState.linkedSubmodels = std::move(submodels);
    renderState.animInstance = AnimMachineInstance(modelRepo.getLoadedOrUnloadedAsset(modularCharacter.baseModel).mAnimMachine->getID());
    for (SubmeshID partId : modularCharacter.partIds) {
        if (partId != INVALID_SUBMESH_ID) {
            const MeshSkeletonData* skeletonData = modelRepo.getSubmeshSkeletonData(partId);
            assert(skeletonData);
            renderState.boneTransformCount += skeletonData->mNumJoints;
            ++renderState.submeshPartCount;
        }
    }
    mTotalJointTransforms += renderState.boneTransformCount;
    mTotalSubmeshParts += renderState.submeshPartCount;
}

void CharacterRenderer::removeCharacterModelInternal(entt::entity entityId) {
    ASSERT_RENDER_THREAD();
    ModelRepository& modelRepo = ModelRepository::get();
    auto it = mCharacterModels.find(entityId);
    assert(it != mCharacterModels.end());

    mTotalJointTransforms -= it->second.boneTransformCount;
    mTotalSubmeshParts -= it->second.submeshPartCount;

    mCharacterModels.erase(it);
}

void CharacterRenderer::addSubmodelInternal(entt::entity entityId, LinkedSubmodelData submodel) {
    ModelRepository& modelRepo = ModelRepository::get();
    auto it = mCharacterModels.find(entityId);
    // Rare case where we reuse the entity ID
    if (it != mCharacterModels.end()) {
        it->second.linkedSubmodels.emplace_back(submodel);
    }
}

void CharacterRenderer::removeSubmodelInternal(entt::entity entityId, LinkedSubmodelData submodel) {
    ModelRepository& modelRepo = ModelRepository::get();
    auto it = mCharacterModels.find(entityId);
    // Rare case where we reuse the entity ID
    if (it != mCharacterModels.end()) {
        for (size_t i = 0; i < it->second.linkedSubmodels.size(); ++i) {
            if (it->second.linkedSubmodels[i] == submodel) {
                it->second.linkedSubmodels[i] = std::move(it->second.linkedSubmodels[it->second.linkedSubmodels.size() - 1]);
                it->second.linkedSubmodels.pop_back();
                return;
            }
        }
    }
    else {
        // TODO: Is this a failure?
        __debugbreak();
    }
}

void CharacterRenderer::onCharacterModelConstruct(entt::registry& registry, entt::entity entity) {
    CharacterModelComponent& cmp = registry.get<CharacterModelComponent>(entity);
    addCharacterModel(entity, cmp.getModel(), cmp.getLinkedSubmodels());
}

void CharacterRenderer::onCharacterModelDestroy(entt::registry& registry, entt::entity entity) {
    removeCharacterModel(entity);
}

void CharacterRenderer::onCharacterModelSubmodelAdded(entt::entity entityId, LinkedSubmodelData submodel) {
     if (IS_RENDER_THREAD()) {
         addSubmodelInternal(entityId, submodel);
     }
     else {
         mModelsToUpdate.enqueue({ entityId, submodel, CharacterModelUpdateType::AddSubmodel });
     }
}

void CharacterRenderer::onCharacterModelSubmodelRemoved(entt::entity entityId, LinkedSubmodelData submodel) {
    if (IS_RENDER_THREAD()) {
        addSubmodelInternal(entityId, submodel);
    }
    else {
        mModelsToUpdate.enqueue({ entityId, submodel, CharacterModelUpdateType::RemoveSubmodel });
    }
}
