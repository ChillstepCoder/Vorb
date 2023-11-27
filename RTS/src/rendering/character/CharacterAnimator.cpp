#include "stdafx.h"
#include "CharacterAnimator.h"

#include "ozz/base/containers/vector.h"
#include "ozz/base/span.h"
#include "ozz/base/maths/simd_math.h"
#include "ozz/base/maths/soa_transform.h"
#include "ozz/animation/runtime/local_to_model_job.h"
#include "ozz/animation/runtime/sampling_job.h"
#include "ozz/animation/runtime/blending_job.h"

bool CharacterAnimator::updateAnimation(const MeshSkeletonData& skeletonData, CharacterAnimState& animState, CharacterLocomotionMode locomotionMode, const ModelDef& modelDef, ozz::vector<ozz::math::Float4x4>& models, f32 elapsedSec) {

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
    num_skinning_matrices = skeletonData.mNumJoints;

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
        blend_job.layers = ozz::span{ layers, size_t(totalLayers) };
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

void CharacterAnimator::updateAnimationStates(CharacterAnimState& animState, CharacterLocomotionMode locomotionMode, f32 elapsedSec) {

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