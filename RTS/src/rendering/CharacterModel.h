#pragma once

struct SpriteData;
class SpriteRepository;

#include <Vorb/graphics/Texture.h>

#include "definitions/AnimMachineDef.h"

#include <ozz/animation/runtime/sampling_job.h>

struct ModelDef;
struct RigDef;

enum CharacterModelTextureIndex {
	CHARACTER_MODEL_TEXTURE_FRONT = 0,
	CHARACTER_MODEL_TEXTURE_SIDE  = 1,
	CHARACTER_MODEL_TEXTURE_BACK  = 2
};

enum class AnimTrackFlags : ui8 {
    IS_LOOPING    = 1 << 0,
    IS_UPPER_BODY = 1 << 1,
    IS_FADING_OUT = 1 << 2,
    IS_FADING_IN  = 1 << 3,
    IS_ACTIVE     = 1 << 4
};

struct AnimTrack {
    std::unique_ptr<ozz::animation::SamplingJob::Context> mContext; // TODO: Pool allocator
    f32 mDuration = 1.0f;
    f32 mTime = 0.0f;
    f32 mWeight = 0.0f;
    ui16 mFadeWeight = UINT16_MAX; // Packed into ui16 to keep AnimTrack at 24 bytes
    ui8 mFadeSpeed = UINT8_MAX; // Packed into ui8
    BitFlags<AnimTrackFlags> mFlags = BitFlags<AnimTrackFlags>(AnimTrackFlags::IS_LOOPING);

    bool isActive() const { return mWeight > 0.0001f && mFlags.isBitSet(AnimTrackFlags::IS_ACTIVE); }
    bool isDone() const { return mTime >= mDuration; }
    void update(f32 elapsedSec);
    f32 getTotalWeight() const { return mWeight * ((f32)mFadeWeight / UINT16_MAX); }
};
static_assert(sizeof(AnimTrack) == 24, "Keep small");

constexpr ui32 NUM_ANIM_TRACKS = e_cast(AnimMachineState::COUNT);
struct AnimState {
    AnimTrack mTracks[NUM_ANIM_TRACKS];
};

// TODO: File name
struct CharacterModelComponent {
    const ModelDef* mModel = nullptr;
    AnimState mAnimState;

    void init(const ModelDef* model);
    void setAnimTrackWeight(AnimMachineState currentState, f32 weight);
};