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

struct AnimTrack {
    std::unique_ptr<ozz::animation::SamplingJob::Context> mContext; // TODO: Pool allocator
    f32 mDuration = 1.0f;
    f32 mTime = 0.0f;
    f32 mWeight = 0.0f;
    bool mIsLooping = true;
    bool mIsUpperBody = false; // TODO: Flags

    bool isDone() const { return mTime >= mDuration; }
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
    void setAnimTrack(AnimMachineState currentState, f32 weight);
};