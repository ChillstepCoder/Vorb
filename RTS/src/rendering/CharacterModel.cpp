#include "stdafx.h"
#include "CharacterModel.h"

#include <Vorb/graphics/TextureCache.h>

#include "definitions/ModelDef.h"
#include "definitions/RigDef.h"

#include "rendering/SpriteRepository.h"

// TODO: Remove, this is used for the static sShadowTexture load
#include "CharacterRenderer.h"
#include <ozz/animation/runtime/animation.h>

constexpr ui16 DEFAULT_ANIM_TRACK_FLAGS[NUM_ANIM_TRACKS] = {
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
    e_cast(AnimTrackFlags::IS_LOOPING),// IDLE_COMBAT
};
static_assert(NUM_ANIM_TRACKS == 11u, "Update any defaults");

void CharacterModelComponent::init(const ModelDef* model) {
    mModel = model;
    for (ui32 i = 0; i < NUM_ANIM_TRACKS; ++i) {
        AnimTrack& track = mAnimState.mTracks[i];
        const ozz::animation::Animation* anim = mModel->mAnimMachine->mAnimsArray[i];
        if (anim) {
            track.mDuration = mModel->mAnimMachine->mAnimsArray[i]->duration();
        }
        mAnimState.mTracks[i].mFlags.setBits((AnimTrackFlags)DEFAULT_ANIM_TRACK_FLAGS[i]);
        // TODO: Better context allocation
        track.mContext = std::make_unique<ozz::animation::SamplingJob::Context>();
        track.mContext->Resize(mModel->mRig->mSkeleton.num_joints());
    }
    // Init to idle state engaged
    mAnimState.mTracks[e_cast(AnimMachineState::IDLE)].mWeightScale = 1.0f;
    mAnimState.mTracks[e_cast(AnimMachineState::IDLE)].mWeight = MAX_ANIM_FADE_WEIGHT;
    // Init one shot anim track
    mAnimState.mCurrentOneShotTrack.mContext = std::make_unique<ozz::animation::SamplingJob::Context>();
    mAnimState.mCurrentOneShotTrack.mContext->Resize(mModel->mRig->mSkeleton.num_joints());
}

void CharacterModelComponent::setAnimTrackWeight(AnimMachineState currentState, f32 weightScale) {

    AnimTrack& track = mAnimState.mTracks[e_cast(currentState)];
    track.mWeightScale = weightScale;
}

void CharacterModelComponent::updateFootstepAlpha(f32 elapsedSec, LocomotionMode currentLocomotionMode) {
    // TODO: Allow per model specification
    const f32 cycleDuration = FOOTSTEP_CYCLE_DURATION_SEC[e_cast(currentLocomotionMode)];
    assert(cycleDuration);
    mFootstepAlpha += elapsedSec / cycleDuration;
    if (mFootstepAlpha > 1.0f) {
        mFootstepAlpha -= (int)mFootstepAlpha;
    }
}

void CharacterModelComponent::playOneShotAnimation(const ozz::animation::Animation* animation) {

    mAnimState.mCurrentOneShotTrack.mTime = 0.0f;
    mAnimState.mCurrentOneShotTrack.mDuration = animation->duration();
    mAnimState.mCurrentOneShotTrack.fadeIn(0.2);
    mAnimState.mCurrentOneShotAnimation = animation;

}

constexpr f32 FADE_SPEED_SCALE = 0.5f;
constexpr f32 MAX_FADE_DURATION = 1.0f / FADE_SPEED_SCALE;
constexpr f32 MIN_FADE_DURATION = 1.0f / (FADE_SPEED_SCALE * 255.0f);

void AnimTrack::fadeIn(f32 fadeTime) {
    // TODO: Tmp
    mWeight = 1.0f;
    // Don't fade in if we already are
    if (mFlags.isBitSet(AnimTrackFlags::IS_FADING_IN) || mWeight == MAX_ANIM_FADE_WEIGHT) {
        return;
    }

    f32 fadeSpeed = glm::min(1.0f / (fadeTime * FADE_SPEED_SCALE), 255.0f);
    mFadeSpeed = ui8(fadeSpeed);
    assert(mFadeSpeed != 0);
    mFlags.setBits(AnimTrackFlags::IS_FADING_IN, AnimTrackFlags::IS_ACTIVE);
    mFlags.clearBit(AnimTrackFlags::IS_FADING_OUT);
}

void AnimTrack::fadeOut(f32 fadeTime) {

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
                mTime = 0.0f;
                mFlags.clearBit(AnimTrackFlags::IS_ACTIVE);
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
