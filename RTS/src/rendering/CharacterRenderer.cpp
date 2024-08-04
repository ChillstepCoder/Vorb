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

struct CharacterRendererCharacterState {
    //CharacterAnimState animState;
    AnimMachineInstance mAnimInstance;
    const CharacterRenderState* renderStateThisFrame = nullptr;
};

CharacterRenderer::CharacterRenderer() :
    mShaderHandle(MaterialShaderRepository::get().getAssetHandle(CStrToken("character"))) {
}

CharacterRenderer::~CharacterRenderer() {
    if (mRegisteredECS) {
        mRegisteredECS->on_construct<CharacterModelComponent>().disconnect<&CharacterRenderer::onCharacterModelConstruct>(this);
        mRegisteredECS->on_destroy<CharacterModelComponent>().disconnect<&CharacterRenderer::onCharacterModelDestroy>(this);
    }
}

void CharacterRenderer::onWorldBegin(World& world) {
    ASSERT_GAME_THREAD();
    // Only register once
    if (!mRegisteredECS) {
        mRegisteredECS = &world.getECS().mRegistry;
        mRegisteredECS->on_construct<CharacterModelComponent>().connect<&CharacterRenderer::onCharacterModelConstruct>(this);
        mRegisteredECS->on_destroy<CharacterModelComponent>().connect<&CharacterRenderer::onCharacterModelDestroy>(this);
    }
}

void CharacterRenderer::frameBegin() {
    ASSERT_RENDER_THREAD();

    constexpr ui32 BULK_DEQUEUE_SIZE = 16;
    CharacterModelUpdateData modelsToUpdate[BULK_DEQUEUE_SIZE];

    if (const size_t count = mModelsToUpdate.try_dequeue_bulk(modelsToUpdate, BULK_DEQUEUE_SIZE)) {
        for (size_t i = 0; i < count; ++i) {
            CharacterModelUpdateData& updateData = modelsToUpdate[i];
            if (updateData.isAdd) {
                addCharacterModelInternal(updateData.entityId, updateData.modelId);
            }
            else {
                removeCharacterModelInternal(updateData.entityId, updateData.modelId);
            }
        }
    }
}

void CharacterRenderer::addCharacterModel(entt::entity entityId, AssetID modelId) {
    if (IS_RENDER_THREAD()) {
        addCharacterModelInternal(entityId, modelId);
    }
    else {
        mModelsToUpdate.enqueue({ entityId, modelId, true });
    }
}

void CharacterRenderer::removeCharacterModel(entt::entity entityId, AssetID modelId) {
    if (IS_RENDER_THREAD()) {
        removeCharacterModelInternal(entityId, modelId);
    }
    else {
        mModelsToUpdate.enqueue({ entityId, modelId, false });
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

void CharacterRenderer::renderCharacters(const Camera3D& camera, const std::vector<CharacterRenderState>& characters, f32 elapsedSec, f32 frameAlpha) {
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

        // Render all characters with this model
        for (auto& [entityId, characterState] : renderData.entityCharacterModels) {
            // We require a render state to render, new entities may not have one
            if (!characterState.renderStateThisFrame) [[unlikely]] {
                continue;
            }

            const CharacterRenderState& character = *characterState.renderStateThisFrame;
            const RigDef& rig = characterState.mAnimInstance.getRig();

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

                const f32 distSQ = glm::length2(offset);
                MeshLODLevel lod = lodParams.selectLOD(distSQ);
                const f32 angle = character.mRotation;

                // Convert degrees to radians for angles
                const f32 angleZ = DEG_TO_RAD(90.0f) + angle;
                const f32 angleX = DEG_TO_RAD(90.0f);

                // Precompute sine and cosine values
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
                glm::mat4 transform(
                    cosZ * tmp, sinZ * tmp, 0.0f, 0.0f,
                    -sinZ * cosX, cosZ * cosX, sinX, 0.0f,
                    sinZ * sinX, -cosZ * sinX, cosX, 0.0f,
                    offset.x, offset.y, offset.z, 1.0f
                );

                glUniformMatrix4fv(modelTransformUniform, 1, false, &transform[0][0]);

                for (ui32 i = 0; i < modelDef.mNumMeshes; ++i) {
                    const SkeletalMesh& skeletalMesh = modelDef.getSkeletalMesh(i);
                    const MeshSkeletonData& skelData = skeletalMesh.getSkeletonData();

                    // Skin animation to mesh
                    ozz::math::Float4x4 skinningBuffer[MAX_JOINTS_IN_RIG];
                    OzzMatrixSpan skinningMatrices(skinningBuffer, skelData.mNumJoints);
                    if (!SkeletalAnimator::skinModelMatricesToMesh(ozz::make_span(modelMatrices), skelData, skinningMatrices)) {
                        panic("Anim skinning fail!");
                    }

                    glUniformMatrix4fv(boneUniform, skelData.mNumJoints, false, (const GLfloat*)skinningBuffer);

                    skeletalMesh.bindSkeletalModelAttribs();

                    // TODO: Indirect?
                    MeshDrawer::draw(skeletalMesh.mGpuData, lod);
                }
            }
        }
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

void CharacterRenderer::onCharacterModelConstruct(entt::registry& registry, entt::entity entity) {
    ModelID modelId = registry.get<CharacterModelComponent>(entity).modelId;
    LOG_DEBUG("Added model ID {} for entity {}", modelId, e_cast(entity));
    addCharacterModel(entity, registry.get<CharacterModelComponent>(entity).modelId);
}

void CharacterRenderer::onCharacterModelDestroy(entt::registry& registry, entt::entity entity) {
    LOG_DEBUG("Destroying model for entity {}", e_cast(entity));
    removeCharacterModel(entity, registry.get<CharacterModelComponent>(entity).modelId);
}
