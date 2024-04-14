#include "stdafx.h"
#include "CharacterRenderer.h"

#include "ecs/component/PhysicsComponent.h"
#include "ecs/component/CharacterControlComponent.h"

#include "resources/IAssetRepository.h"
#include "rendering/MaterialShaderDef.h"

#include "rendering/MaterialShaderRepository.h"

#include "rendering/MaterialRenderer.h"
#include "rendering/TileVertex.h"
#include "rendering/renderstate/CharacterRenderState.h"
#include "rendering/mesh/MeshDrawer.h"

#include "resources/ModelRepository.h"
#include "resources/AnimationRepository.h"
#include "resources/ResourceManager.h"

#include "world/World.h"
#include "ecs/IEntityComponentSystem.h"

#include "options/DebugOptions.h"

#include "math/Random.h"

#include "camera/Camera3D.h"

#include "definitions/RigDef.h"
#include "ozz/base/containers/vector.h"
#include "ozz/base/maths/simd_math.h"


CharacterRenderer::CharacterRenderer() :
    mShaderHandle(MaterialShaderRepository::get().getAssetHandle(CStrToken("character"))) {

    mCharacterAnimator = std::make_unique<CharacterAnimator>();
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
        mCharacterAnimator->playOneShotAnimation(it->second->animState, &animDef->animation);
    }
}

void CharacterRenderer::renderCharacters(const Camera3D& camera, const std::vector<CharacterRenderState>& characters, f32 elapsedSec, f32 frameAlpha) {
    UNUSED(frameAlpha);
    PROFILE_FUNCTION();

    const MaterialShaderDef* shaderDef = mShaderHandle->tryGetLoadedAsset();
    if (!shaderDef) return;

    // We will render sorted by model ID, so pair up all render states this frame
    for (const auto& character : characters) {
        auto&& it = mEntityCharacterRenderData.find(character.mEntityID);
        if (it != mEntityCharacterRenderData.end()) {
            it->second->renderStateThisFrame = const_cast<CharacterRenderState*>(&character);
        }
    }

    // TODO: UBO
    MaterialRenderer::bindMaterialShaderForRender(*shaderDef);
    VGUniform offsetUniform = shaderDef->mProgram.getUniform("unOffset");
    VGUniform modelTransformUniform = shaderDef->mProgram.getUniform("unModelTransform");
    VGUniform boneUniform = shaderDef->mProgram.getUniform("unBoneTransforms[0]");
    for (auto& [modelID, renderData] : mModelRenderData) {

        // Lazy initialize
        if (renderData.needsInitialize) [[unlikely]] {
            if (!renderData.handle) {
                renderData.handle = ModelRepository::get().getAssetHandle(modelID);
            }
            if (const ModelDef* modelDefPtr = renderData.handle->tryGetLoadedAsset()) {
                renderData.animatorData.rig = modelDefPtr->mRig;
                renderData.animatorData.machine = modelDefPtr->mAnimMachine;
                assert(renderData.animatorData.rig);
                assert(renderData.animatorData.machine);
                for (auto& [entityId, characterState] : renderData.entityCharacterModels) {
                    mCharacterAnimator->initializeCharacterAnimState(characterState.animState, *modelDefPtr);
                }
                renderData.needsInitialize = false;
            }
            else {
                continue;
            }
        }


        const ModelDef& modelDef = renderData.handle->getLoadedAsset();

        // Render all characters with this model
        for (auto& [entityId, characterState] : renderData.entityCharacterModels) {
            const CharacterRenderState& character = *characterState.renderStateThisFrame;

            const f32v3& position = character.mPos;
            const f32 angle = character.mRotation;
            // TODO: Optimize
            f32m4 transform(1.0f);
            transform = glm::rotate(transform, DEG_TO_RAD(90.0f) + angle, f32v3(0.0f, 0.0f, 1.0f));
            transform = glm::rotate(transform, DEG_TO_RAD(90.0f), f32v3(1.0f, 0.0f, 0.0f));

            const f32v3 offset = position - camera.getPosition();
            glUniform3fv(offsetUniform, 1, &offset.x);
            glUniformMatrix4fv(modelTransformUniform, 1, false, &transform[0][0]);

            // Allocates skinning matrices.
            for (ui32 i = 0; i < modelDef.mNumMeshes; ++i) {
                const SkeletalMesh& skeletalMesh = modelDef.getSkeletalMesh(i);
                const MeshSkeletonData& skelData = skeletalMesh.getSkeletonData();

                if (ozz::vector<ozz::math::Float4x4>* skinningMatrices = mCharacterAnimator->updateAnimation(renderData.animatorData, skelData, characterState.animState, character.mLocomotionMode, elapsedSec)) {
                    // Draw animated
                  
                    glUniformMatrix4fv(boneUniform, skelData.mNumJoints, false, (const GLfloat*)&(*skinningMatrices)[0].cols);

                    // TODO: Indirect?
                    MeshDrawer::draw(skeletalMesh.mGpuData);
                }
                else {
                    // INVALID ANIMATION
                    ozz::vector<ozz::math::Float4x4> tmpMatrices(skelData.mNumJoints, ozz::math::Float4x4::identity());
                    glUniformMatrix4fv(boneUniform, skelData.mNumJoints, false, (const GLfloat*)&tmpMatrices[0].cols);

                    MeshDrawer::draw(skeletalMesh.mGpuData);
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
        const ModelDef& modelDef = renderData.handle->getLoadedAsset();
        mCharacterAnimator->initializeCharacterAnimState(newState.animState, modelDef);
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
