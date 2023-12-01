#pragma once
// TODO: Hardware skinning? https://github.com/ConfettiFX/The-Forge/tree/master/Examples_3/Unit_Tests/src/28_Skinning

class ModelDef;
class AnimMachineDef;
class RigDef;
struct MeshSkeletonData;
struct CharacterAnimState;

#include "rendering/model/AnimationConst.h"
#include "character/CharacterConst.h"

#include <ozz/base/containers/vector.h>
#include <ozz/base/maths/simd_math.h>
#include <ozz/base/maths/soa_transform.h>
#include <ozz/animation/runtime/sampling_job.h>
#include <ozz/base/span.h>
#include <ozz/animation/runtime/local_to_model_job.h>
#include <ozz/animation/runtime/sampling_job.h>
#include <ozz/animation/runtime/blending_job.h>

struct CharacterAnimatorModelData {
    const AnimMachineDef* machine = nullptr;
    const RigDef* rig = nullptr;
};

enum class AnimTrackFlags : ui8 {
    IS_LOOPING = 1 << 0,
    IS_UPPER_BODY = 1 << 1,
    IS_FADING_OUT = 1 << 2,
    IS_FADING_IN = 1 << 3,
    IS_ACTIVE = 1 << 4,
    IS_SYNCED_TO_FEET = 1 << 5,
    HOLD_END_POSE = 1 << 6,
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

struct CharacterAnimState {

    AnimTrack mTracks[NUM_ANIM_STATE_TRACKS];
    AnimTrack mCurrentOneShotTrack;
    const ozz::animation::Animation* mCurrentOneShotAnimation = nullptr;
    ModelID mModelID = INVALID_MODEL_ID;
    f32 mFootstepAlpha;
    ui8 mPrimaryStateTrack = UINT8_MAX;
    CharacterLocomotionMode mPrevLocomotionMode = CharacterLocomotionMode::IDLE;
};

class CharacterAnimator {
public:
    // Return NULL on fail
    ozz::vector<ozz::math::Float4x4>* updateAnimation(const CharacterAnimatorModelData& data, const MeshSkeletonData& skeletonData, CharacterAnimState& animState, CharacterLocomotionMode locomotionMode, f32 elapsedSec);

    void initializeCharacterAnimState(CharacterAnimState& animState, const ModelDef& modelDef);
    void playOneShotAnimation(CharacterAnimState& animState, const ozz::animation::Animation* animation);
    void setAnimTrackWeight(CharacterAnimState& animState, AnimMachineState currentState, f32 weightScale);
protected:
    void fadeInStateTrack(CharacterAnimState& animState, AnimMachineState state, f32 fadeDuration);
    void updateFootstepAlpha(CharacterAnimState& animState, f32 elapsedSec, CharacterLocomotionMode currentLocomotionMode);
    void updateAnimationStates(CharacterAnimState& animState, CharacterLocomotionMode locomotionMode, f32 elapsedSec);

    ozz::vector<ozz::math::Float4x4> models;
    ozz::vector<ozz::math::Float4x4> skinningMatrices;
    ozz::vector<ozz::math::SoaTransform> locals[NUM_ANIM_STATE_TRACKS + 1];
    ozz::vector<ozz::math::SoaTransform> blendedLocals;
};

