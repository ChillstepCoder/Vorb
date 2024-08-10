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
    //CharacterAnimState animState;
    AnimMachineInstance mAnimInstance;
    const CharacterRenderState* renderStateThisFrame = nullptr;
    std::vector<LinkedSubmodel> mLinkedSubmodels;
};

CharacterRenderer::CharacterRenderer() :
    mShaderHandle(MaterialShaderRepository::get().getAssetHandle(CStrToken("character"))) {
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

    if (const size_t count = mModelsToUpdate.try_dequeue_bulk(modelsToUpdate, BULK_DEQUEUE_SIZE)) {
        for (size_t i = 0; i < count; ++i) {
            CharacterModelUpdateData& updateData = modelsToUpdate[i];
            switch (updateData.type) {
                case CharacterModelUpdateType::Add:
                    addCharacterModelInternal(updateData.entityId, updateData.modelId);
                    break;
                case CharacterModelUpdateType::Remove:
                    removeCharacterModelInternal(updateData.entityId, updateData.modelId);
                    break;
                case CharacterModelUpdateType::AddSubmodel:
                    addSubmodelInternal(updateData.entityId, updateData.submodel);
                    break;
                case CharacterModelUpdateType::RemoveSubmodel:
                    removeSubmodelInternal(updateData.entityId, updateData.submodel);
                    break;
                default:
                    assert(false);
                    break;

            }
            static_assert(e_count(CharacterModelUpdateType) == 4);
        }
    }
}

void CharacterRenderer::addCharacterModel(entt::entity entityId, AssetID modelId) {
    if (IS_RENDER_THREAD()) {
        addCharacterModelInternal(entityId, modelId);
    }
    else {
        mModelsToUpdate.enqueue({ entityId, modelId, CharacterModelUpdateType::Add });
    }
}

void CharacterRenderer::removeCharacterModel(entt::entity entityId, AssetID modelId) {
    if (IS_RENDER_THREAD()) {
        removeCharacterModelInternal(entityId, modelId);
    }
    else {
        mModelsToUpdate.enqueue({ entityId, modelId, CharacterModelUpdateType::Remove });
    }
}

void CharacterRenderer::playOneShotAnimation(entt::entity entityId, AssetID animationId) {
    auto&& it = mEntityCharacterRenderData.find(entityId);
    // TODO: Ensure?
    assert(it != mEntityCharacterRenderData.end());
    if (it != mEntityCharacterRenderData.end()) {

        // TODO: Allow lazy load anim? hmmm prob not?
        const AnimationDef* animDef = AnimationRepository::get().tryGetLoadedAsset(animationId);
        if (!animDef) panic("Tried to play one shot anim {} that was not loaded", animationId);
        it->second->mAnimInstance.tryPlayOneShot(*animDef);
    }
}

void CharacterRenderer::renderCharactersAndGatherSubmodels(const Camera3D& camera, const std::vector<CharacterRenderState>& characters, f32 elapsedSec, f32 frameAlpha, std::vector<DynamicModelInstanceState>& outLinkedSubmodels) {
    UNUSED(frameAlpha);
    PROFILE_FUNCTION();

    const MaterialShaderDef* shaderDef = mShaderHandle->tryGetLoadedAsset();
    if (!shaderDef) return;

    // We will render sorted by model ID, so pair up all render states this frame
    // TODO: Also sort anim machine?
    for (const auto& character : characters) {
        auto&& it = mEntityCharacterRenderData.find(character.mEntityID);
        if (it != mEntityCharacterRenderData.end()) {
            it->second->renderStateThisFrame = const_cast<CharacterRenderState*>(&character);
        }
    }

    // TODO: Camera culling

    // TODO: UBO
    MaterialRenderer::bindMaterialShaderForRender(*shaderDef);
    VGUniform modelTransformUniform = shaderDef->mProgram.getUniform("unModelTransform");
    VGUniform boneUniform = shaderDef->mProgram.getUniform("unBoneTransforms[0]");
    for (auto& [modelID, renderData] : mModelRenderData) {

        // Lazy initialize
        if (renderData.needsInitialize) [[unlikely]] {
            if (!renderData.handle) {
                renderData.handle = ModelRepository::get().getAssetHandle(modelID);
            }
            if (const ModelDef* modelDefPtr = renderData.handle->tryGetLoadedAsset()) {
                // Create all anim instances
                AssetID machineId = modelDefPtr->mAnimMachine->getID();
                for (auto& [entityId, characterState] : renderData.entityCharacterModels) {
                    characterState.mAnimInstance = AnimMachineInstance(machineId);

                    // Update any linked submodel cached bones
                    const RigDef& rig = characterState.mAnimInstance.getRig();
                    for (LinkedSubmodel& submodel : characterState.mLinkedSubmodels) {
                        auto&& jit = rig.mJointNameToIndex.find(submodel.attachBone);
                        assert(jit != rig.mJointNameToIndex.end());
                        submodel.cachedAttachBoneIndex = jit->second;
                    }
                }
                renderData.needsInitialize = false;
            }
            else {
                // Still loading assets
                continue;
            }
        }


        const ModelDef& modelDef = renderData.handle->getLoadedAsset();
        const ModelLodParams& lodParams = ModelRepository::get().getLodParams(modelID);
        const RigDef& rig = *modelDef.mRig;

        const i32 numEntities = (i32)renderData.entityCharacterModels.size();
        const i32 transformsNeeded = modelDef.mTotalSubmeshJointTransformsNeeded;
        // Update transforms capacity if needed
        if (!renderData.boneTransformsBuffer) {
            renderData.boneTransformsBuffer = std::make_unique<GpuStreamingDataBuffer>(transformsNeeded * (numEntities + TRANSFORMS_PADDING_SIZE), sizeof(f32m4));
        } else if (renderData.boneTransformsBuffer->getMaxElements() < transformsNeeded * numEntities) {
            // Grow
            renderData.boneTransformsBuffer->setMaxElements((transformsNeeded + 1) * (numEntities + TRANSFORMS_PADDING_SIZE));
        } else if (renderData.boneTransformsBuffer->getMaxElements() > transformsNeeded * (numEntities + TRANSFORMS_PADDING_SIZE * 4)) {
            // Shrink
            renderData.boneTransformsBuffer->setMaxElements(transformsNeeded * (numEntities + TRANSFORMS_PADDING_SIZE));
        }
        LOG_INFO("  TRANSFORMS DATA SIZE {} mb", 3.0f * (f32)renderData.boneTransformsBuffer->getMaxElements() * sizeof(f32m4) / 1024.0f / 1024.0f);

        //f32m4* transformsPtr = (f32m4*)renderData.boneTransformsBuffer->frameBeginAndGetDataForUpdate();

        // Render all characters with this model
        for (auto& [entityId, characterState] : renderData.entityCharacterModels) {
            // We require a render state to render, new entities may not have one
            if (!characterState.renderStateThisFrame) [[unlikely]] {
                continue;
            }

            const CharacterRenderState& character = *characterState.renderStateThisFrame;

            const f32v3& position = character.mPos;
            const bool isVisible = camera.sphereIsVisible(position, lodParams.boundingSphereRadius);

            // TODO: combine with CharacterRenderState?
            AnimVariables variables;
            variables.locomotionMode = character.mLocomotionMode;
            variables.velocity2d = character.mVelocity2D;
            variables.speed = glm::length(character.mVelocity2D);
            ozz::math::Float4x4 modelsBuffer[MAX_JOINTS_IN_RIG];
            OzzMatrixSpan modelMatrices(modelsBuffer, rig.mSkeleton.num_joints());

            // Update animation TODO: Multithreaded?
            // When not visible we pass null output buffer, signaling that we dont want to update bones, only tick the anim states
            characterState.mAnimInstance.update(elapsedSec, variables, isVisible ? modelMatrices : OzzMatrixSpan(nullptr, (size_t)0));

            if (isVisible) {
               

                // Manually set world translation (TODO: Can set this on transform initialize for less instructions)
                const f32v3 offset = position - camera.getPosition();

                const f32 angle = character.mRotation;

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
                const glm::mat4 cameraRelTransform(
                    cosZ * tmp, sinZ * tmp, 0.0f, 0.0f,
                    -sinZ * cosX, cosZ * cosX, sinX, 0.0f,
                    sinZ * sinX, -cosZ * sinX, cosX, 0.0f,
                    offset.x, offset.y, offset.z, 1.0f
                );

                glUniformMatrix4fv(modelTransformUniform, 1, false, &cameraRelTransform[0][0]);

                const f32 distSQ = glm::length2(offset);
                MeshLODLevel lod = lodParams.selectLOD(distSQ);

                // TODO: We should batch render these
                for (ui32 i = 0; i < modelDef.mNumMeshes; ++i) {
                    const SkeletalMesh& skeletalMesh = modelDef.getSkeletalMesh(i);
                    const MeshSkeletonData& skelData = skeletalMesh.getSkeletonData();

                    // Skin animation to mesh
                    ozz::math::Float4x4 skinningBuffer[MAX_JOINTS_IN_RIG];
                    OzzMatrixSpan skinningMatrices(skinningBuffer, skelData.mNumJoints);
                    if (!SkeletalAnimator::skinModelMatricesToMesh(ozz::make_span(modelMatrices), skelData, skinningMatrices)) {
                        panic("Anim skinning fail!");
                    }

                    // TODO: We shouldn't do this for each submesh
                    glBindBufferBase(GL_UNIFORM_BUFFER, BUFFER_BASE_MODEL_VARIANT_DATA_UBO, skeletalMesh.mVariantDataUbo);
                    glUniformMatrix4fv(boneUniform, skelData.mNumJoints, false, (const GLfloat*)skinningBuffer);

                    skeletalMesh.bindSkeletalModelAttribs();

                    // TODO: Indirect?
                    MeshDrawer::draw(skeletalMesh.mGpuData, lod);
                }


                // Submodels
                for (LinkedSubmodel& submodel : characterState.mLinkedSubmodels) {
                    AssetHandlePtr<ModelDef> submodelHandle = ModelRepository::get().getAssetHandle(submodel.submodelId);
                    if (const ModelDef* def = submodelHandle->tryGetLoadedAsset()) {

                        const f32m4 boneTransform = std::bit_cast<f32m4>(modelMatrices[submodel.cachedAttachBoneIndex]);

                        DynamicModelInstanceState& submodelState = outLinkedSubmodels.emplace_back();
                        submodelState.modelId = submodel.submodelId;

                        const f64m4 worldTransform(
                            cameraRelTransform[0][0], cameraRelTransform[0][1], 0.0f, 0.0f,
                            cameraRelTransform[1][0], cameraRelTransform[1][1], cameraRelTransform[1][2], 0.0f,
                            cameraRelTransform[2][0], cameraRelTransform[2][1], cameraRelTransform[2][2], 0.0f,
                            position.x, position.y, position.z, 1.0f
                        );
                        f64q rotation;
                        f64v3 worldPosition;
                        decomposeMatrix(worldTransform * f64m4(boneTransform), worldPosition, rotation);
                        submodelState.positionXY = worldPosition;
                        submodelState.positionZ = worldPosition.z;
                        submodelState.orientation = rotation;
                    }
                }
            }
        }
        //renderData.boneTransformsBuffer->flushDataAndIncrementFrame(1 /*TODO real count*/);
    }
}

CharacterRendererCharacterState* CharacterRenderer::tryGetCharacterRenderStateForDebug(entt::entity entityId) {
    ASSERT_RENDER_THREAD();
    for (auto& it : mModelRenderData) {
        auto it2 = it.second.entityCharacterModels.find(entityId);
        if (it2 != it.second.entityCharacterModels.end()) {
            return &it2->second;
        }
    }
    return nullptr;
}

void CharacterRenderer::addCharacterModelInternal(entt::entity entityId, AssetID modelId) {
    ASSERT_RENDER_THREAD();
    CharacterModelRendererData& renderData = mModelRenderData[modelId];
    CharacterRendererCharacterState& newState = renderData.entityCharacterModels.emplace(entityId, CharacterRendererCharacterState()).first->second;
    // Only init if we aren't already pending a full init
    if (!renderData.needsInitialize) {
        AssetID machineId = renderData.handle->getLoadedAsset().mAnimMachine->getID();
        newState.mAnimInstance = AnimMachineInstance(machineId);
    }
    assert(!mEntityCharacterRenderData.contains(entityId));
    mEntityCharacterRenderData[entityId] = &newState;
}

void CharacterRenderer::removeCharacterModelInternal(entt::entity entityId, AssetID modelId) {
    ASSERT_RENDER_THREAD();
    CharacterModelRendererData& renderData = mModelRenderData[modelId];
    renderData.entityCharacterModels.erase(entityId);
    mEntityCharacterRenderData.erase(entityId);
    // TODO: Deallocate handle if needed
}

void CharacterRenderer::addSubmodelInternal(entt::entity entityId, LinkedSubmodel submodel) {
    auto&& it = mEntityCharacterRenderData.find(entityId);
    if (it != mEntityCharacterRenderData.end()) {
        if (it->second->mAnimInstance.isValid()) {
            const RigDef& rig = it->second->mAnimInstance.getRig();
            auto&& jit = rig.mJointNameToIndex.find(submodel.attachBone);
            assert(jit != rig.mJointNameToIndex.end());
            submodel.cachedAttachBoneIndex = jit->second;
        }
        it->second->mLinkedSubmodels.push_back(submodel);
    }
    else {
        // TODO: Is this a failure?
        __debugbreak();
    }
}

void CharacterRenderer::removeSubmodelInternal(entt::entity entityId, LinkedSubmodel submodel) {
    auto&& it = mEntityCharacterRenderData.find(entityId);
    if (it != mEntityCharacterRenderData.end()) {
        for (size_t i = 0; i < it->second->mLinkedSubmodels.size(); ++i) {
            if (it->second->mLinkedSubmodels[i] == submodel) {
                it->second->mLinkedSubmodels[i] = std::move(it->second->mLinkedSubmodels[it->second->mLinkedSubmodels.size() - 1]);
                it->second->mLinkedSubmodels.pop_back();
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
    ModelID modelId = registry.get<CharacterModelComponent>(entity).modelId;
    LOG_DEBUG("Added model ID {} for entity {}", modelId, e_cast(entity));
    addCharacterModel(entity, registry.get<CharacterModelComponent>(entity).modelId);
}

void CharacterRenderer::onCharacterModelDestroy(entt::registry& registry, entt::entity entity) {
    LOG_DEBUG("Destroying model for entity {}", e_cast(entity));
    removeCharacterModel(entity, registry.get<CharacterModelComponent>(entity).modelId);
}

void CharacterRenderer::onCharacterModelSubmodelAdded(entt::entity entityId, LinkedSubmodel submodel) {
     if (IS_RENDER_THREAD()) {
         addSubmodelInternal(entityId, submodel);
     }
     else {
         mModelsToUpdate.enqueue({ entityId, submodel, CharacterModelUpdateType::AddSubmodel });
     }
}

void CharacterRenderer::onCharacterModelSubmodelRemoved(entt::entity entityId, LinkedSubmodel submodel) {
    if (IS_RENDER_THREAD()) {
        addSubmodelInternal(entityId, submodel);
    }
    else {
        mModelsToUpdate.enqueue({ entityId, submodel, CharacterModelUpdateType::RemoveSubmodel });
    }
}
