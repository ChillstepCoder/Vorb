#pragma once

struct SpriteData;

#include <Vorb/graphics/Texture.h>

#include "definitions/AnimMachineDef.h"

#include "ecs/component/CharacterControlComponent.h"
#include <ozz/animation/runtime/sampling_job.h>

struct ModelDef;
struct RigDef;

enum CharacterModelTextureIndex {
	CHARACTER_MODEL_TEXTURE_FRONT = 0,
	CHARACTER_MODEL_TEXTURE_SIDE  = 1,
	CHARACTER_MODEL_TEXTURE_BACK  = 2
};

enum class AnimTrackFlags : ui8 {
    IS_LOOPING        = 1 << 0,
    IS_UPPER_BODY     = 1 << 1,
    IS_FADING_OUT     = 1 << 2,
    IS_FADING_IN      = 1 << 3,
    IS_ACTIVE         = 1 << 4,
    IS_SYNCED_TO_FEET = 1 << 5,
    HOLD_END_POSE     = 1 << 6,
};

constexpr ui16 MAX_ANIM_FADE_WEIGHT = UINT16_MAX;

struct AnimTrack {
    std::unique_ptr<ozz::animation::SamplingJob::Context> mContext; // TODO: Pool allocator
    f32 mDuration = 1.0f;
    f32 mTime = 0.0f;
    f32 mWeightScale = 1.0f;
    ui16 mWeight = 0; // Packed into ui16 to keep AnimTrack at 24 bytes
    ui8 mFadeSpeed = UINT8_MAX; // Packed into ui8
    BitFlags<AnimTrackFlags> mFlags;

    void fadeIn(f32 fadeTime);
    void fadeOut(f32 fadeTime);
    bool isActive() const { return mWeightScale && mFlags.isBitSet(AnimTrackFlags::IS_ACTIVE); }
    bool isDone() const { return mTime >= mDuration; }
    void update(f32 elapsedSec, f32 footstepAlpha);
    f32 getTotalWeight() const { return mWeightScale * ((f32)mWeight / MAX_ANIM_FADE_WEIGHT); }
};
static_assert(sizeof(AnimTrack) == 24, "Keep small");

constexpr ui32 NUM_ANIM_STATE_TRACKS = e_cast(AnimMachineState::COUNT);
struct AnimState {

    void fadeInStateTrack(AnimMachineState state, f32 fadeDuration);

    AnimTrack mTracks[NUM_ANIM_STATE_TRACKS];
    AnimTrack mCurrentOneShotTrack;
    ui8 mPrimaryStateTrack = UINT8_MAX;
    const ozz::animation::Animation* mCurrentOneShotAnimation = nullptr;
};

// TODO: File name
struct CharacterModelComponent {
    const ModelDef* mModel = nullptr;
    AnimState mAnimState;
    f32 mFootstepAlpha;
    LocomotionMode mPrevLocomotionMode = LocomotionMode::IDLE;
    // TODO: Flags
    bool mIsPlayer = true;

    void init(const ModelDef* model);
    void setAnimTrackWeight(AnimMachineState currentState, f32 weightScale);
    void updateFootstepAlpha(f32 elapsedSec, LocomotionMode currentLocomotionMode);
    void playOneShotAnimation(const ozz::animation::Animation* animation);
};