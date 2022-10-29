#include "stdafx.h"
#include "CharacterModel.h"

#include <Vorb/graphics/TextureCache.h>

#include "definitions/ModelDef.h"
#include "definitions/RigDef.h"

// TODO: Remove, this is used for the static sShadowTexture load
#include "CharacterRenderer.h"
#include <ozz/animation/runtime/animation.h>
#include <boost/pool/singleton_pool.hpp>

constexpr f32 FADE_SPEED_SCALE = 0.5f;
constexpr f32 MAX_FADE_DURATION = 1.0f / FADE_SPEED_SCALE;
constexpr f32 MIN_FADE_DURATION = 1.0f / (FADE_SPEED_SCALE * 255.0f);

void AnimTrack::fadeIn(f32 fadeTime) {
    assert(IS_RENDER_THREAD());
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
    assert(IS_RENDER_THREAD());

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
    assert(IS_RENDER_THREAD());
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

void AnimState::fadeInStateTrack(AnimMachineState state, f32 fadeDuration) {
    assert(IS_RENDER_THREAD());
    mTracks[e_cast(state)].fadeIn(fadeDuration);
    mPrimaryStateTrack = (ui8)state;
}

void AnimState::setAnimTrackWeight(AnimMachineState currentState, f32 weightScale) {
    assert(IS_RENDER_THREAD());
    AnimTrack& track = mTracks[e_cast(currentState)];
    track.mWeightScale = weightScale;
}

void AnimState::updateFootstepAlpha(f32 elapsedSec, CharacterLocomotionMode currentLocomotionMode) {
    assert(IS_RENDER_THREAD());
    // TODO: Allow per model specification
    const f32 cycleDuration = FOOTSTEP_CYCLE_DURATION_SEC[e_cast(currentLocomotionMode)];
    assert(cycleDuration);
    mFootstepAlpha += elapsedSec / cycleDuration;
    if (mFootstepAlpha > 1.0f) {
        mFootstepAlpha -= (int)mFootstepAlpha;
    }
}

void AnimState::playOneShotAnimation(const ozz::animation::Animation* animation) {
    assert(IS_RENDER_THREAD());

    mCurrentOneShotTrack.mTime = 0.0f;
    mCurrentOneShotTrack.mDuration = animation->duration();
    mCurrentOneShotTrack.fadeIn(0.2);
    mCurrentOneShotAnimation = animation;
}

// TODO: Don't include in server project
struct animstate_task_pool {};
using singleton_task_pool = boost::singleton_pool<animstate_task_pool, sizeof(AnimState), boost::default_user_allocator_new_delete, boost::details::pool::null_mutex, 256u>;

void* AnimState::operator new(size_t count)
{
    UNUSED(count);
    return singleton_task_pool::malloc();
}

void AnimState::operator delete(void* pointer, size_t size) {
    UNUSED(size);
    return singleton_task_pool::free(pointer);
}
