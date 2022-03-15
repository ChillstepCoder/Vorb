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
}

void CharacterModelComponent::setAnimTrack(AnimMachineState currentState, f32 weight) {

    AnimTrack& track = mAnimState.mTracks[e_cast(currentState)];
    track.mTime = 0.0f;
    track.mDuration = mModel->mAnimMachine->mAnimsArray[e_cast(currentState)]->duration();
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

