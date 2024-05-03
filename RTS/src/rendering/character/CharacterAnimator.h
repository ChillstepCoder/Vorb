#pragma once
// TODO: Hardware skinning? https://github.com/ConfettiFX/The-Forge/tree/master/Examples_3/Unit_Tests/src/28_Skinning

class ModelDef;
class AnimMachineDef;
class RigDef;
struct MeshSkeletonData;
struct CharacterAnimState;

#include "rendering/model/skeletal/SkeletalAnimationSampleContext.h"

#include "rendering/model/AnimationConst.h"
#include "character/CharacterLocomotionMode.h"

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

struct CharacterAnimState {
    std::unique_ptr<SkeletalAnimationSampleContext> mCurrentOneShotContext; // TODO: Pool allocate?
    //const ozz::animation::Animation* mCurrentOneShotAnimation = nullptr;
    ModelID mModelID = INVALID_MODEL_ID;
    //f32 mFootstepAlpha;
    //CharacterLocomotionMode mPrevLocomotionMode = CharacterLocomotionMode::IDLE;
    // TODO: Anim machine state
};

class CharacterAnimator {
public:
    // Return NULL on fail
    ozz::vector<ozz::math::Float4x4>* updateAnimation(const CharacterAnimatorModelData& data, const MeshSkeletonData& skeletonData, CharacterAnimState& animState, CharacterLocomotionMode locomotionMode, f32 elapsedSec);

    void initializeCharacterAnimState(CharacterAnimState& animState, const ModelDef& modelDef);
    void playOneShotAnimation(CharacterAnimState& animState, const ozz::animation::Animation* animation);
protected:/*
    void updateFootstepAlpha(CharacterAnimState& animState, f32 elapsedSec, CharacterLocomotionMode currentLocomotionMode);*/

    ozz::vector<ozz::math::Float4x4> models;
    ozz::vector<ozz::math::Float4x4> skinningMatrices;/*
    ozz::vector<ozz::math::SoaTransform> locals[NUM_ANIM_STATE_TRACKS + 1];*/
    ozz::vector<ozz::math::SoaTransform> blendedLocals;
};
