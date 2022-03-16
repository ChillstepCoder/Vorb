#include "stdafx.h"
#include "CharacterModel.h"

#include <Vorb/graphics/TextureCache.h>

#include "definitions/ModelDef.h"
#include "definitions/RigDef.h"

#include "rendering/SpriteRepository.h"

// TODO: Remove, this is used for the static sShadowTexture load
#include "CharacterRenderer.h"
#include <ozz/animation/runtime/animation.h>

void CharacterModelComponent::init(const ModelDef* model) {
    mModel = model;
    for (ui32 i = 0; i < NUM_ANIM_TRACKS; ++i) {
        AnimTrack& track = mAnimState.mTracks[i];
        const ozz::animation::Animation* anim = mModel->mAnimMachine->mAnimsArray[i];
        if (anim) {
            track.mDuration = mModel->mAnimMachine->mAnimsArray[i]->duration();
        }
    }
    // Init to idle state engaged
    mAnimState.mTracks[e_cast(AnimMachineState::IDLE)].mWeight = 1.0f;
    mAnimState.mTracks[e_cast(AnimMachineState::IDLE)].mFlags.setBit(AnimTrackFlags::IS_ACTIVE);
}

void CharacterModelComponent::setAnimTrackWeight(AnimMachineState currentState, f32 weight) {

    AnimTrack& track = mAnimState.mTracks[e_cast(currentState)];
    track.mWeight = weight;
    if (track.mWeight) {
        // TODO: Is this lazy init really ok?
        if (!track.mContext) {
            track.mContext = std::make_unique<ozz::animation::SamplingJob::Context>();
        }
        if (track.mContext->max_soa_tracks() != mModel->mRig->mSkeleton.num_joints()) {
            track.mContext->Resize(mModel->mRig->mSkeleton.num_joints());
        }
    }
}

constexpr f32 FADE_SPEED_SCALE = 0.5f;

void AnimTrack::update(f32 elapsedSec) {
    // Update fade
    if (mFlags.isBitSet(AnimTrackFlags::IS_FADING_IN)) {
        const f32 fadeAmount = mFadeSpeed * elapsedSec * FADE_SPEED_SCALE;
        f32 currentFade = (f32)mFadeWeight / UINT16_MAX;
        currentFade += fadeAmount;
        if (currentFade >= 1.0f) {
            mFadeWeight = UINT16_MAX;
            mFlags.clearBit(AnimTrackFlags::IS_FADING_IN);
        }
        else {
            mFadeWeight = currentFade * UINT16_MAX;
        }
    }
    else if (mFlags.isBitSet(AnimTrackFlags::IS_FADING_OUT)) {
        const f32 fadeAmount = mFadeSpeed * elapsedSec * FADE_SPEED_SCALE;
        f32 currentFade = (f32)mFadeWeight / UINT16_MAX;
        currentFade -= fadeAmount;
        if (currentFade <= 0.0f) {
            mFadeWeight = 0;
            mFlags.clearMaskBits(e_cast(AnimTrackFlags::IS_FADING_OUT) | e_cast(AnimTrackFlags::IS_ACTIVE));
        }
        else {
            mFadeWeight = currentFade * UINT16_MAX;
        }
    }
    // Update anim time
    mTime += elapsedSec;
    if (isDone()) {
        if (mFlags.isBitSet(AnimTrackFlags::IS_LOOPING)) {
            mTime -= mDuration;
        }
        else {
            mTime = mDuration;
        }
    }
}
