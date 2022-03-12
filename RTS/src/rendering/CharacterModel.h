#pragma once

struct SpriteData;
class SpriteRepository;

#include <Vorb/graphics/Texture.h>

#include "definitions/AnimMachineDef.h"

#include <ozz/animation/runtime/sampling_job.h>

class ModelDef;
struct RigDef;

enum CharacterModelTextureIndex {
	CHARACTER_MODEL_TEXTURE_FRONT = 0,
	CHARACTER_MODEL_TEXTURE_SIDE  = 1,
	CHARACTER_MODEL_TEXTURE_BACK  = 2
};

struct AnimTrack {
    std::unique_ptr<ozz::animation::SamplingJob::Context> mContext;
    AnimMachineState mState = AnimMachineState::IDLE;
    bool mIsLooping = true;
    bool mIsUpperBody = false;
    f32 mDuration = 1.0f;
    f32 mTime = 0.0f;
    f32 mWeight = 0.0f;

    bool isDone() const { return mTime >= mDuration; }
};

constexpr ui32 NUM_ANIM_TRACKS = 2;
struct AnimState {
    AnimTrack mTracks[NUM_ANIM_TRACKS];

    bool isBlending() const { return mTracks[0].mState != mTracks[1].mState && mTracks[0].mWeight != 0.0f && mTracks[1].mWeight != 0.0f; };
};

// TODO: File name
struct CharacterModelComponent {
    const ModelDef* mModel = nullptr;
    AnimState mAnimState;

    void setAnimTrack(ui32 trackIndex, AnimMachineState currentState, f32 weight);
};