#include "stdafx.h"
#include "CharacterRenderer.h"

#include "ecs/component/PhysicsComponent.h"
#include "ecs/component/CharacterControlComponent.h"

#include "rendering/MaterialManager.h"

#include "rendering/MaterialRenderer.h"
#include "rendering/TileVertex.h"
#include "rendering/renderstate/CharacterRenderState.h"

#include "resources/ModelRepository.h"
#include "resources/AnimationRepository.h"
#include "resources/ResourceManager.h"

#include "options/DebugOptions.h"

#include "math/Random.h"

#include "camera/Camera3D.h"

#include "definitions/RigDef.h"
#include "ozz/base/containers/vector.h"
#include "ozz/base/span.h"
#include "ozz/base/maths/simd_math.h"
#include "ozz/base/maths/soa_transform.h"
#include "ozz/animation/runtime/local_to_model_job.h"
#include "ozz/animation/runtime/sampling_job.h"
#include "ozz/animation/runtime/blending_job.h"

constexpr ui16 DEFAULT_ANIM_TRACK_FLAGS[NUM_ANIM_STATE_TRACKS] = {
    e_cast(AnimTrackFlags::IS_SYNCED_TO_FEET) | e_cast(AnimTrackFlags::IS_LOOPING),// WALK_LEFT
    e_cast(AnimTrackFlags::IS_SYNCED_TO_FEET) | e_cast(AnimTrackFlags::IS_LOOPING),// WALK_RIGHT
    e_cast(AnimTrackFlags::IS_SYNCED_TO_FEET) | e_cast(AnimTrackFlags::IS_LOOPING),// WALK_FRONT
    e_cast(AnimTrackFlags::IS_SYNCED_TO_FEET) | e_cast(AnimTrackFlags::IS_LOOPING),// WALK_BACK
    e_cast(AnimTrackFlags::IS_SYNCED_TO_FEET) | e_cast(AnimTrackFlags::IS_LOOPING),// RUN_LEFT
    e_cast(AnimTrackFlags::IS_SYNCED_TO_FEET) | e_cast(AnimTrackFlags::IS_LOOPING),// RUN_RIGHT
    e_cast(AnimTrackFlags::IS_SYNCED_TO_FEET) | e_cast(AnimTrackFlags::IS_LOOPING),// RUN_FRONT
    e_cast(AnimTrackFlags::IS_SYNCED_TO_FEET) | e_cast(AnimTrackFlags::IS_LOOPING),// RUN_BACK
    e_cast(AnimTrackFlags::IS_SYNCED_TO_FEET) | e_cast(AnimTrackFlags::IS_LOOPING),// SPRINT_FRONT
    e_cast(AnimTrackFlags::IS_ACTIVE) | e_cast(AnimTrackFlags::IS_LOOPING),// IDLE
    e_cast(AnimTrackFlags::IS_LOOPING), // IDLE_COMBAT
    e_cast(AnimTrackFlags::IS_LOOPING), // FALLING
    e_cast(AnimTrackFlags::HOLD_END_POSE) | e_cast(AnimTrackFlags::IS_LOOPING),// JUMP
    0u, // LAND
};
static_assert(NUM_ANIM_STATE_TRACKS == 14u, "Update any defaults");

CharacterRenderer::CharacterRenderer() :
    mMaterial(Services::ResourceManager::ref().getMaterialManager().getMaterial("character")) {
    mEntityCharacterModels.reserve(256);
}

CharacterRenderer::~CharacterRenderer() {

}

void CharacterRenderer::addCharacterModel(entt::entity entityId, ui32 modelId) {
    assert(mEntityCharacterModels.find(entityId) == mEntityCharacterModels.end());
    assert(modelId != INVALID_MODEL_ID);
    std::unique_ptr<AnimState> animState = std::make_unique<AnimState>();
    const ModelDef& modelDef = Services::ResourceManager::ref().getModelRepository().getModelDef(modelId);
    animState->mModelID = modelId;
    for (ui32 i = 0; i < NUM_ANIM_STATE_TRACKS; ++i) {
        AnimTrack& track = animState->mTracks[i];
        const ozz::animation::Animation* anim = modelDef.mAnimMachine->mAnimsArray[i];
        if (anim) {
            track.mDuration = modelDef.mAnimMachine->mAnimsArray[i]->duration();
        }
        animState->mTracks[i].mFlags.setBits((AnimTrackFlags)DEFAULT_ANIM_TRACK_FLAGS[i]);
        // TODO: Better context allocation
        track.mContext = std::make_unique<ozz::animation::SamplingJob::Context>();
        track.mContext->Resize(modelDef.mRig->mSkeleton.num_joints());
    }
    // Init to idle state engaged
    animState->mTracks[e_cast(AnimMachineState::IDLE)].mWeightScale = 1.0f;
    animState->mTracks[e_cast(AnimMachineState::IDLE)].mWeight = MAX_ANIM_FADE_WEIGHT;
    // Init one shot anim track
    animState->mCurrentOneShotTrack.mContext = std::make_unique<ozz::animation::SamplingJob::Context>();
    animState->mCurrentOneShotTrack.mContext->Resize(modelDef.mRig->mSkeleton.num_joints());
    mEntityCharacterModels[entityId] = std::move(animState);
}

void CharacterRenderer::removeCharacterModel(entt::entity entityId) {
    auto&& it = mEntityCharacterModels.find(entityId);
    assert(it != mEntityCharacterModels.end());
    it->second.reset();
    mEntityCharacterModels.erase(it);
}

void CharacterRenderer::playOneShotAnimation(entt::entity entityId, ui32 animationId) {
    auto&& it = mEntityCharacterModels.find(entityId);
    // TODO: Ensure
    assert(it != mEntityCharacterModels.end());
    if (it != mEntityCharacterModels.end()) {
        const ozz::animation::Animation& anim = Services::ResourceManager::ref().getAnimationRepository().getAnimation(animationId);
        it->second->playOneShotAnimation(&anim);
    }
}

void updateAnimationStates(AnimState& animState, CharacterLocomotionMode locomotionMode, f32 elapsedSec) {

    // Update feel
    animState.updateFootstepAlpha(elapsedSec, locomotionMode);

    constexpr f32 FADE_IN_SLOW = 0.3f;
    constexpr f32 FADE_IN_MEDIUM = 0.2f;
    constexpr f32 FADE_IN_FAST = 0.1f;

    const bool isTransitioning = (locomotionMode != animState.mPrevLocomotionMode);
    animState.mPrevLocomotionMode = locomotionMode;

    // Update transition
    if (isTransitioning) {

        // Fade out previous state
        if (animState.mPrimaryStateTrack != UINT8_MAX) {
            animState.mTracks[animState.mPrimaryStateTrack].fadeOut(FADE_IN_SLOW);
        }

        // TODO: Array lookup mapping instead of switch?
        switch (locomotionMode) {
            case CharacterLocomotionMode::IDLE: {
                animState.fadeInStateTrack(AnimMachineState::IDLE, FADE_IN_SLOW);
                break;
            }
            case CharacterLocomotionMode::WALK: {
                animState.fadeInStateTrack(AnimMachineState::WALK_FRONT, FADE_IN_SLOW);
                break;
            }
            case CharacterLocomotionMode::RUN: {
                animState.fadeInStateTrack(AnimMachineState::RUN_FRONT, FADE_IN_SLOW);
                break;
            }
            case CharacterLocomotionMode::SPRINT: {
                animState.fadeInStateTrack(AnimMachineState::SPRINT_FRONT, FADE_IN_SLOW);
                break;
            }
            case CharacterLocomotionMode::DODGE: {

                break;
            }
            case CharacterLocomotionMode::BEGIN_JUMP:
                //assert(false && "We should never try to play Begin Jump anim");
                std::cout << "BEGIN JUMP ASSERT FAIL\n";
                break;
            case CharacterLocomotionMode::JUMPING: {
                animState.fadeInStateTrack(AnimMachineState::JUMPING, FADE_IN_FAST);
                break;
            }
            case CharacterLocomotionMode::FALLING: {
                animState.fadeInStateTrack(AnimMachineState::FALLING, FADE_IN_MEDIUM);
                break;
            }
            case CharacterLocomotionMode::LANDING: {
                animState.fadeInStateTrack(AnimMachineState::LANDING, FADE_IN_FAST);
                break;
            }
            default:
                assert(false && "Invalid LocomotionMode");
                break;

        }
    }
    static_assert(e_cast(CharacterLocomotionMode::COUNT) == 9, "Update anim mapping");

}

bool updateAnimation(AnimState& animState, CharacterLocomotionMode locomotionMode, const ModelDef& modelDef, ozz::vector<ozz::math::Float4x4>& models, f32 elapsedSec) {

    // Speed blend, run/walk/sprint
    updateAnimationStates(animState, locomotionMode, elapsedSec);

    // Buffer of local transforms as sampled from animation_.
    // TODO: Stack allocate these with joint limits and stop using make_span? Or if too large, shared heap memory
    ozz::vector<ozz::math::SoaTransform> locals[NUM_ANIM_STATE_TRACKS + 1];
    f32 blendWeights[NUM_ANIM_STATE_TRACKS + 1];
    ozz::vector<ozz::math::SoaTransform> blendedLocals;

    assert(modelDef.mAnimMachine);
    assert(modelDef.mRig);

    const AnimMachineDef& machine = *modelDef.mAnimMachine;
    const RigDef& rig = *modelDef.mRig;

    // Animation and skinning

    // Allocates runtime buffers.
    // TODO: Cache
    const int numSoaJoints = rig.mSkeleton.num_soa_joints();
    blendedLocals.resize(numSoaJoints);
    const int numJoints = rig.mSkeleton.num_joints();
    models.resize(numJoints);

    // TODO: cache
    ui8 num_skinning_matrices = 0;
    for (ui32 i = 0; i < modelDef.mModel.getNumMeshes(); ++i) {
        num_skinning_matrices =
            std::max(num_skinning_matrices, modelDef.mModel.getMeshes()[i].getNumJoints());
    }

    ui32 numValidTracks = 0;
    for (ui32 i = 0; i < NUM_ANIM_STATE_TRACKS; ++i) {
        AnimTrack& currentTrack = animState.mTracks[i];
        // If our weightScale made us inactive, make sure to fully disable
        if (!currentTrack.isActive()) {
            // Always force fadeout
            if (currentTrack.mFlags.isBitSet(AnimTrackFlags::IS_FADING_OUT)) {
                currentTrack.mWeight = 0;
                currentTrack.mFlags.clearBit(AnimTrackFlags::IS_FADING_OUT);
            }
            continue;
        }
        // Allocate buffers
        locals[numValidTracks].resize(numSoaJoints);
        // Sample animation
        ozz::animation::SamplingJob sampling_job;
        sampling_job.animation = machine.mAnimsArray[i];
        sampling_job.context = currentTrack.mContext.get();
        sampling_job.ratio = currentTrack.mTime / currentTrack.mDuration;
        sampling_job.output = make_span(locals[numValidTracks]);
        blendWeights[numValidTracks] = currentTrack.getTotalWeight();
        ++numValidTracks;
        if (!sampling_job.Run()) {
            pError("Sampling job error");
            return false;
        }

        // Increment timers
        currentTrack.update(elapsedSec, animState.mFootstepAlpha);
    }

    // One shot animation
    f32 oneShotWeight = 0.0f;
    AnimTrack& oneShotTrack = animState.mCurrentOneShotTrack;
    if (oneShotTrack.isActive()) {
        oneShotWeight = oneShotTrack.getTotalWeight();
        // Allocate buffers
        locals[numValidTracks].resize(numSoaJoints);
        // Sample animation
        ozz::animation::SamplingJob sampling_job;
        sampling_job.animation = animState.mCurrentOneShotAnimation;
        sampling_job.context = oneShotTrack.mContext.get();
        sampling_job.ratio = oneShotTrack.mTime / oneShotTrack.mDuration;
        sampling_job.output = make_span(locals[numValidTracks]);
        blendWeights[numValidTracks] = oneShotWeight;
        if (!sampling_job.Run()) {
            pError("Sampling job error");
            return false;
        }

        // Increment timers
        oneShotTrack.update(elapsedSec, animState.mFootstepAlpha);
    }

    // Converts from local space to model space matrices.
    ozz::animation::LocalToModelJob ltm_job;
    ltm_job.skeleton = &rig.mSkeleton;

    // Blending
    if (numValidTracks > 1 || oneShotWeight) {

        const f32 inverseOneShotWeightMult = 1.0f - oneShotWeight;
        int totalLayers = 0;
        // Prepares blending layers.
        ozz::animation::BlendingJob::Layer layers[(NUM_ANIM_STATE_TRACKS + 1) * 2]; // Account for splitting layers

        // While one shots are active, blending is more complex as we must split lower and upper body blending
        if (oneShotWeight) {
            // Split all layers into upper and lower body portions based on the one shot weight
            if (numValidTracks > 0) {
                if (oneShotWeight == 1.0f) {
                    // If we are at full weight, we have no upper body layers
                    for (ui32 i = 0; i < numValidTracks; ++i) {
                        layers[totalLayers].transform = make_span(locals[i]);
                        layers[totalLayers].weight = blendWeights[i];
                        layers[totalLayers].joint_weights = make_span(rig.mLowerBodyJointWeights);
                        ++totalLayers;
                    }
                }
                else {
                    // Split into two layers for upper and lower portion
                    const f32 upperBodyWeight = 1.0f - oneShotWeight;
                    for (ui32 i = 0; i < numValidTracks; ++i) {
                        // Lower body
                        layers[totalLayers].transform = make_span(locals[i]);
                        layers[totalLayers].weight = blendWeights[i];
                        layers[totalLayers].joint_weights = make_span(rig.mLowerBodyJointWeights);
                        ++totalLayers;

                        // Upper body
                        layers[totalLayers].transform = make_span(locals[i]);
                        layers[totalLayers].weight = blendWeights[i] * upperBodyWeight;
                        layers[totalLayers].joint_weights = make_span(rig.mUpperBodyJointWeights);
                        ++totalLayers;
                    }
                }

                // The final layer is the one shot layer, flag it upper body only if we are in motion
                // TODO: Need to fade this in as well to prevent pop?
                if (locomotionMode != CharacterLocomotionMode::IDLE) {
                    layers[totalLayers].joint_weights = make_span(rig.mUpperBodyJointWeights);
                }
            }

            // Add one shot locals
            layers[totalLayers].transform = make_span(locals[numValidTracks]);
            layers[totalLayers].weight = blendWeights[numValidTracks];
            ++totalLayers;
        }
        else {
            // No one shot, standard, cheap full blending for each anim
            for (ui32 i = 0; i < numValidTracks; ++i) {
                layers[totalLayers].transform = make_span(locals[i]);
                layers[totalLayers].weight = blendWeights[i];
                ++totalLayers;
            }
        }

        // Setups blending job.
        ozz::animation::BlendingJob blend_job;
        blend_job.threshold = 0.015f;
        blend_job.layers = ozz::span{layers, size_t(totalLayers)};
        blend_job.rest_pose = rig.mSkeleton.joint_rest_poses();
        blend_job.output = make_span(blendedLocals);

        // Blends.
        if (!blend_job.Run()) {
            pError("Blending job error");
            return false;
        }
        ltm_job.input = make_span(blendedLocals);
    }
    else {
        ltm_job.input = make_span(locals[0]);
    }

    // Run the final job
    if (numValidTracks) {
        ltm_job.output = make_span(models);
        if (!ltm_job.Run()) {
            pError("Local to model job error");
            return false;
        }
        return true;
    }
    return false;

}

void CharacterRenderer::renderCharacters(const Camera3D& camera, const std::vector<CharacterRenderState>& characters, f32 elapsedSec, f32 frameAlpha) {
    UNUSED(frameAlpha);

    // TODO: UBO
    VGUniform offsetUniform = mMaterial->mProgram.getUniform("unOffset");
    VGUniform modelTransformUniform = mMaterial->mProgram.getUniform("unModelTransform");
    VGUniform diffuseTextureUniform = mMaterial->mProgram.getUniform("unDiffuse");
    VGUniform normalTextureUniform = mMaterial->mProgram.getUniform("unNormal");
    VGUniform specularTextureUniform = mMaterial->mProgram.getUniform("unSpecular");
    VGUniform scaleUniform = mMaterial->mProgram.getUniform("unScale");
    VGUniform boneUniform = mMaterial->mProgram.getUniform("unBoneTransforms[0]");

    for (const auto& character : characters) {
        // Get physics info
        const f32v3& position = character.mPos;
        const f32 angle = character.mRotation;

        auto&& it = mEntityCharacterModels.find(character.mEntityID);
        if (it != mEntityCharacterModels.end()) {
            AnimState& animState = *it->second;
            const ModelDef& modelDef = Services::ResourceManager::ref().getModelRepository().getModelDef(animState.mModelID);
            ui32 nextTextureIndex = 0;
            MaterialRenderer::bindMaterialForRender(*mMaterial, &nextTextureIndex);
            glUniform1i(diffuseTextureUniform, nextTextureIndex);
            glUniform1i(normalTextureUniform, nextTextureIndex + 1);
            glUniform1i(specularTextureUniform, nextTextureIndex + 2);
            glUniform1f(scaleUniform, 1.0f);

            // TODO: Optimize
            f32m4 transform(1.0f);
            transform = glm::rotate(transform, DEG_TO_RAD(180.0f) -angle, f32v3(0.0f, 0.0f, 1.0f));
            transform = glm::rotate(transform, DEG_TO_RAD(90.0f), f32v3(1.0f, 0.0f, 0.0f));

            const f32v3 offset = position - camera.getPosition();
            glUniform3fv(offsetUniform, 1, &offset.x);
            glUniformMatrix4fv(modelTransformUniform, 1, false, &transform[0][0]);

            // Buffer of model space matrices.
            ozz::vector<ozz::math::Float4x4> models;
            // Buffer of skinning matrices, result of the joint multiplication of the
            // inverse bind pose with the model space matrix.
            ozz::vector<ozz::math::Float4x4> skinningMatrices;
            // Allocates skinning matrices.
            skinningMatrices.resize(modelDef.mModel.getNumSkinningMatrices());

            if (updateAnimation(animState, character.mLocomotionMode, modelDef, models, elapsedSec)) {
                // Draw animated
                for (ui32 i = 0; i < modelDef.mModel.getNumMeshes(); ++i) {
                    const auto& mesh = modelDef.mModel.getMeshes()[i];
                    const ozz::math::Float4x4* bindPoses = mesh.getInverseBindPoses();
                    for (size_t i = 0; i < mesh.getNumJoints(); ++i) {
                        skinningMatrices[i] = models[mesh.getJointRemaps()[i]] * bindPoses[i];
                    }
                    glActiveTexture(GL_TEXTURE0 + nextTextureIndex);
                    glBindTexture(GL_TEXTURE_2D, mesh.getDiffuseTexture());
                    glActiveTexture(GL_TEXTURE0 + nextTextureIndex + 1);
                    glBindTexture(GL_TEXTURE_2D, mesh.getNormalTexture());
                    glActiveTexture(GL_TEXTURE0 + nextTextureIndex + 2);
                    glBindTexture(GL_TEXTURE_2D, mesh.getSpecularTexture());
                    glUniformMatrix4fv(boneUniform, mesh.getNumJoints(), false, (const GLfloat*)&skinningMatrices[0].cols);

                    mesh.draw(mMaterial->mProgram);
                }
            }
            else {
                // INVALID ANIMATION
                // Draw T pose
                for (ui32 i = 0; i < modelDef.mModel.getNumMeshes(); ++i) {
                    const auto& mesh = modelDef.mModel.getMeshes()[i];
                    for (size_t i = 0; i < mesh.getNumJoints(); ++i) {
                        skinningMatrices[i] = ozz::math::Float4x4::identity();
                    }
                    glActiveTexture(GL_TEXTURE0 + nextTextureIndex);
                    glBindTexture(GL_TEXTURE_2D, mesh.getDiffuseTexture());
                    glActiveTexture(GL_TEXTURE0 + nextTextureIndex + 1);
                    glBindTexture(GL_TEXTURE_2D, mesh.getNormalTexture());
                    glActiveTexture(GL_TEXTURE0 + nextTextureIndex + 2);
                    glBindTexture(GL_TEXTURE_2D, mesh.getSpecularTexture());
                    glUniformMatrix4fv(boneUniform, mesh.getNumJoints(), false, (const GLfloat*)&skinningMatrices[0].cols);

                    mesh.draw(mMaterial->mProgram);
                }
            }
        }
    }
}

// Prevent rounding errors, 0.0001 is half a pixel
constexpr f32 UV_EPSILON = 0.0001f;
constexpr f32 UV_EPSILON_2 = 2.0f * UV_EPSILON;
