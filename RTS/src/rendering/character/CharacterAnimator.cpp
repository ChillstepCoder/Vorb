#include "stdafx.h"
#include "CharacterAnimator.h"
#include "rendering/CharacterModel.h"

#include "definitions/RigDef.h"
#include "definitions/ModelDef.h"
#include "definitions/AnimMachineDef.h"

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

constexpr f32 FADE_SPEED_SCALE = 0.5f;
constexpr f32 MAX_FADE_DURATION = 1.0f / FADE_SPEED_SCALE;
constexpr f32 MIN_FADE_DURATION = 1.0f / (FADE_SPEED_SCALE * 255.0f);

void AnimTrack::fadeIn(f32 fadeTime) {
    ASSERT_RENDER_THREAD();
    // TODO: Tmp
    mWeight = 1.0f;
    // Don't fade in if we already are
    if (mFlags.isBitSet(AnimTrackFlags::IS_FADING_IN) || mWeight == MAX_ANIM_FADE_WEIGHT) {
        return;
    }

    if (!mFlags.isBitSet(AnimTrackFlags::IS_SYNCED_TO_FEET)) {
        mTime = 0.0f;
    }

    const f32 fadeSpeed = glm::min(1.0f / (fadeTime * FADE_SPEED_SCALE), 255.0f);
    mFadeSpeed = ui8(fadeSpeed);
    assert(mFadeSpeed != 0);
    mFlags.setBits(AnimTrackFlags::IS_FADING_IN, AnimTrackFlags::IS_ACTIVE);
    mFlags.clearBit(AnimTrackFlags::IS_FADING_OUT);
}

void AnimTrack::fadeOut(f32 fadeTime) {
    ASSERT_RENDER_THREAD();

    // Don't fade out if we already are
    if (mFlags.isBitSet(AnimTrackFlags::IS_FADING_OUT) || mWeight == 0) {
        return;
    }

    f32 fadeSpeed = glm::min(1.0f / (fadeTime * FADE_SPEED_SCALE), 255.0f);
    mFadeSpeed = ui8(fadeSpeed);
    mFlags.setBit(AnimTrackFlags::IS_FADING_OUT);
    mFlags.clearBit(AnimTrackFlags::IS_FADING_IN);
}

void AnimTrack::update(f32 elapsedSec, f32 footstepAlpha) {
    ASSERT_RENDER_THREAD();
    // Update fade
    if (mFlags.isBitSet(AnimTrackFlags::IS_FADING_IN)) {
        const f32 fadeAmount = mFadeSpeed * elapsedSec * FADE_SPEED_SCALE;
        f32 currentFade = (f32)mWeight / MAX_ANIM_FADE_WEIGHT;
        currentFade += fadeAmount;
        if (currentFade >= 1.0f) {
            mWeight = MAX_ANIM_FADE_WEIGHT;
            mFlags.clearBit(AnimTrackFlags::IS_FADING_IN);
        }
        else {
            mWeight = currentFade * MAX_ANIM_FADE_WEIGHT;
        }
    }
    else if (mFlags.isBitSet(AnimTrackFlags::IS_FADING_OUT)) {
        const f32 fadeAmount = mFadeSpeed * elapsedSec * FADE_SPEED_SCALE;
        f32 currentFade = (f32)mWeight / MAX_ANIM_FADE_WEIGHT;
        currentFade -= fadeAmount;
        if (currentFade <= 0.0f) {
            mWeight = 0;
            mFlags.clearMaskBits(e_cast(AnimTrackFlags::IS_FADING_OUT) | e_cast(AnimTrackFlags::IS_ACTIVE));
        }
        else {
            mWeight = currentFade * MAX_ANIM_FADE_WEIGHT;
        }
    }
    // Sync to feet
    if (mFlags.isBitSet(AnimTrackFlags::IS_SYNCED_TO_FEET)) {
        assert(footstepAlpha <= 1.0f);
        mTime = footstepAlpha * mDuration;
    }
    else {
        // Update anim time
        mTime += elapsedSec;
        if (isDone()) {
            if (mFlags.isBitSet(AnimTrackFlags::IS_LOOPING)) {
                mTime -= mDuration;
            }
            else {
                // Check whether we need to hold or end the animation
                if (mFlags.isBitSet(AnimTrackFlags::HOLD_END_POSE)) {
                    mTime = mDuration;
                }
                else {
                    mTime = 0.0f;
                    mFlags.clearBit(AnimTrackFlags::IS_ACTIVE);
                }
            }
        }
        else if (!mFlags.isBitSet(AnimTrackFlags::IS_LOOPING) && !mFlags.isBitSet(AnimTrackFlags::IS_FADING_OUT)) {
            // One shot anims always have a built in 0.2s fade out
            constexpr f32 ONE_SHOT_FADE_OUT_TIME = 0.3f;
            if (mDuration - mTime <= ONE_SHOT_FADE_OUT_TIME) {
                fadeOut(ONE_SHOT_FADE_OUT_TIME);
            }
        }
    }
}

ozz::vector<ozz::math::Float4x4>* CharacterAnimator::updateAnimation(const CharacterAnimatorModelData& data, const MeshSkeletonData& skeletonData, CharacterAnimState& animState, CharacterLocomotionMode locomotionMode, f32 elapsedSec) {

    // Speed blend, run/walk/sprint
    updateAnimationStates(animState, locomotionMode, elapsedSec);

    // Buffer of local transforms as sampled from animation_.
    // TODO: Stack allocate these with joint limits and stop using make_span? Or if too large, shared heap memory
    
    f32 blendWeights[NUM_ANIM_STATE_TRACKS + 1];

    assert(data.machine);
    assert(data.rig);

    const AnimMachineDef& machine = *data.machine;
    const RigDef& rig = *data.rig;

    // Animation and skinning
    const int numSoaJoints = rig.mSkeleton.num_soa_joints();
    const int numJoints = rig.mSkeleton.num_joints();
    blendedLocals.resize(numSoaJoints);
    models.resize(numJoints);

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
            return nullptr;
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
            return nullptr;
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
            return nullptr;
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
            return nullptr;
        }

        // Compute skinning matrices
        const ozz::math::Float4x4* bindPoses = skeletonData.mInverseBindPoses.get();

        skinningMatrices.resize(skeletonData.mNumJoints);
        for (size_t i = 0; i < skeletonData.mNumJoints; ++i) {
            skinningMatrices[i] = models[skeletonData.mJointRemaps[i]] * bindPoses[i];
        }

        return &skinningMatrices;
    }
    return nullptr;

}

void CharacterAnimator::initializeCharacterAnimState(CharacterAnimState& animState, const ModelDef& modelDef) {
    animState.mModelID = modelDef.getID();
    for (ui32 i = 0; i < NUM_ANIM_STATE_TRACKS; ++i) {
        AnimTrack& track = animState.mTracks[i];
        const ozz::animation::Animation* anim = modelDef.mAnimMachine->mAnimsArray[i];
        if (anim) {
            track.mDuration = modelDef.mAnimMachine->mAnimsArray[i]->duration();
        }
        animState.mTracks[i].mFlags.setBits((AnimTrackFlags)DEFAULT_ANIM_TRACK_FLAGS[i]);
        // TODO: Better context allocation
        track.mContext = std::make_unique<ozz::animation::SamplingJob::Context>();
        track.mContext->Resize(modelDef.mRig->mSkeleton.num_joints());
    }
    // Init to idle state engaged
    animState.mTracks[e_cast(AnimMachineStateOLD::IDLE)].mWeightScale = 1.0f;
    animState.mTracks[e_cast(AnimMachineStateOLD::IDLE)].mWeight = MAX_ANIM_FADE_WEIGHT;
    // Init one shot anim track
    animState.mCurrentOneShotTrack.mContext = std::make_unique<ozz::animation::SamplingJob::Context>();
    animState.mCurrentOneShotTrack.mContext->Resize(modelDef.mRig->mSkeleton.num_joints());
}

void CharacterAnimator::updateAnimationStates(CharacterAnimState& animState, CharacterLocomotionMode locomotionMode, f32 elapsedSec) {

    // Update feel
    updateFootstepAlpha(animState, elapsedSec, locomotionMode);

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
                fadeInStateTrack(animState, AnimMachineStateOLD::IDLE, FADE_IN_SLOW);
                break;
            }
            case CharacterLocomotionMode::WALK: {
                fadeInStateTrack(animState, AnimMachineStateOLD::WALK_FRONT, FADE_IN_SLOW);
                break;
            }
            case CharacterLocomotionMode::RUN: {
                fadeInStateTrack(animState, AnimMachineStateOLD::RUN_FRONT, FADE_IN_SLOW);
                break;
            }
            case CharacterLocomotionMode::SPRINT: {
                fadeInStateTrack(animState, AnimMachineStateOLD::SPRINT_FRONT, FADE_IN_SLOW);
                break;
            }
            case CharacterLocomotionMode::DODGE: {

                break;
            }
            case CharacterLocomotionMode::BEGIN_JUMP:
                //assert(false && "We should never try to play Begin Jump anim");
                LOG_CRITICAL("BEGIN JUMP ASSERT FAIL\n");
                panic("BEGIN JUMP ASSERT FAIL");
                break;
            case CharacterLocomotionMode::JUMPING: {
                fadeInStateTrack(animState, AnimMachineStateOLD::JUMPING, FADE_IN_FAST);
                break;
            }
            case CharacterLocomotionMode::FALLING: {
                fadeInStateTrack(animState, AnimMachineStateOLD::FALLING, FADE_IN_MEDIUM);
                break;
            }
            case CharacterLocomotionMode::LANDING: {
                fadeInStateTrack(animState, AnimMachineStateOLD::LANDING, FADE_IN_FAST);
                break;
            }
            default:
                assert(false && "Invalid LocomotionMode");
                break;

        }
    }
    static_assert(e_cast(CharacterLocomotionMode::COUNT) == 10, "Update anim mapping");

}

void CharacterAnimator::playOneShotAnimation(CharacterAnimState& animState, const ozz::animation::Animation* animation) {
    ASSERT_RENDER_THREAD();
    animState.mCurrentOneShotTrack.mTime = 0.0f;
    animState.mCurrentOneShotTrack.mDuration = animation->duration();
    animState.mCurrentOneShotTrack.fadeIn(0.2);
    animState.mCurrentOneShotAnimation = animation;
}

void CharacterAnimator::setAnimTrackWeight(CharacterAnimState& animState, AnimMachineStateOLD currentState, f32 weightScale) {
    ASSERT_RENDER_THREAD();
    AnimTrack& track = animState.mTracks[e_cast(currentState)];
    track.mWeightScale = weightScale;
}

void CharacterAnimator::fadeInStateTrack(CharacterAnimState& animState, AnimMachineStateOLD state, f32 fadeDuration) {
    ASSERT_RENDER_THREAD();
    animState.mTracks[e_cast(state)].fadeIn(fadeDuration);
    animState.mPrimaryStateTrack = (ui8)state;
}

void CharacterAnimator::updateFootstepAlpha(CharacterAnimState& animState, f32 elapsedSec, CharacterLocomotionMode currentLocomotionMode) {
    ASSERT_RENDER_THREAD();
    // TODO: Allow per model specification
    const f32 cycleDuration = FOOTSTEP_CYCLE_DURATION_SEC[e_cast(currentLocomotionMode)];
    assert(cycleDuration);
    animState.mFootstepAlpha += elapsedSec / cycleDuration;
    if (animState.mFootstepAlpha > 1.0f) {
        animState.mFootstepAlpha -= (int)animState.mFootstepAlpha;
    }
}

ozz::vector<ozz::math::Float4x4>* CharacterAnimatorNew::updateAnimation(const CharacterAnimatorModelData& data, const MeshSkeletonData& skeletonData, CharacterAnimStateNew& animState, f32 elapsedSec) {
    return nullptr;
}
