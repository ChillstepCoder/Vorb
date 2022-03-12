#include "stdafx.h"
#include "CharacterModel.h"

#include <Vorb/graphics/TextureCache.h>

#include "definitions/ModelDef.h"
#include "definitions/RigDef.h"

#include "rendering/SpriteRepository.h"

// TODO: Remove, this is used for the static sShadowTexture load
#include "CharacterRenderer.h"
#include <ozz/animation/runtime/animation.h>

void CharacterModelComponent::setAnimTrack(ui32 trackIndex, AnimMachineState currentState, f32 weight) {

    assert(trackIndex < NUM_ANIM_TRACKS);
    AnimTrack& track = mAnimState.mTracks[trackIndex];
    track.mState = currentState;
    track.mTime = 0.0f;
    track.mDuration = mModel->mAnimMachine->mAnimsArray[e_cast(currentState)]->duration();
    track.mWeight = weight;
    if (!track.mContext) {
        track.mContext = std::make_unique<ozz::animation::SamplingJob::Context>();
    }
    if (track.mContext->max_soa_tracks() != mModel->mRig->mSkeleton.num_joints()) {
        track.mContext->Resize(mModel->mRig->mSkeleton.num_joints());
    }
    else {
        track.mContext->Invalidate();
    }
}

