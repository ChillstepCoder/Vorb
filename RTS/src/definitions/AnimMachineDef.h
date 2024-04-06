#pragma once

#include "rendering/model/AnimationConst.h"
#include "rendering/model/skeletal/AnimTransitionCondition.h"

// TODO: REMOVE
#include "rendering/model/skeletal/AnimTransitionConditions.h"

namespace ozz {
    namespace animation {
        class Animation;
    };
};

typedef ui8 AnimStateID;
constexpr auto MAX_ANIM_STATES = std::numeric_limits<AnimStateID>::max();

struct AnimTransitionDef {
    StrToken condition;
    f32 transitionDuration;
    StrToken toState;
};

struct AnimTransition {
    // Deliberately just a single condition for now, as these are custom code driven
    AnimTransitionCondition condition;
    f32 transitionDuration;
    AnimStateID toState;
};

enum class AnimStateType : ui8 {
    AnimSequence,
    Blendspace2D,
    Blendspace3D
};

struct AnimStateDef {
    StrToken name;
    std::vector<AnimTransition> transitions;
    AssetID assetId;
    AnimStateType stateType;
};

// Efficient representation
struct AnimState {
    std::unique_ptr<AnimTransition[]> transitions;
    AssetID assetId;
    ui8 numTransitions;
    AnimStateType stateType;
};

// Make sure order and contents of the animation machine name and animation arrays are the same
class AnimMachineDef : public IAsset {
public:
    DEFAULT_ASSET_CONSTRUCTOR(AnimMachineDef, AssetType::AnimMachine);

    union {
        struct {
            const ozz::animation::Animation* mWalkLeftAnim;
            const ozz::animation::Animation* mWalkRightAnim;
            const ozz::animation::Animation* mWalkFrontAnim;
            const ozz::animation::Animation* mWalkBackAnim;
            const ozz::animation::Animation* mRunLeftAnim;
            const ozz::animation::Animation* mRunRightAnim;
            const ozz::animation::Animation* mRunFrontAnim;
            const ozz::animation::Animation* mRunBackAnim;
            const ozz::animation::Animation* mIdleAnim;
            const ozz::animation::Animation* mIdleCombatAnim;
            const ozz::animation::Animation* mFallingAnim;
            const ozz::animation::Animation* mJumpAnim;
            const ozz::animation::Animation* mLandingAnim;
        };
        const ozz::animation::Animation* mAnimsArray[ANIMATION_MACHINE_ANIMS_COUNT] = {};
    };

    // TODO: USE
    // Editor representation, State 0 is entry state
    std::vector<AnimStateDef> mStateDefs;
    // Efficient representation, State 0 is entry state
    std::vector<AnimState> mStates;
};
static_assert(e_cast(AnimMachineStateOLD::COUNT) == 14, "Update AnimMachineDef and FileData below");


struct AnimMachineDefFileData {
    StrToken mRigName;

    // We will iterate these like an array, make sure they are correct
    StrToken mWalkLeftName; //< This must remain the first element in the list of animation, see AnimationMachineRepository.cpp (loadMachineFile)
    StrToken mWalkRightName;
    StrToken mWalkFrontName;
    StrToken mWalkBackName;
    StrToken mRunLeftName;
    StrToken mRunRightName;
    StrToken mRunFrontName;
    StrToken mRunBackName;
    StrToken mSprintFrontName;
    StrToken mIdleName;
    StrToken mIdleCombatName;
    StrToken mFallingName;
    StrToken mJumpName;
    StrToken mLandingName;
};
static_assert(e_cast(AnimMachineStateOLD::COUNT) == 14, "Make sure to update ANIMATION_MACHINE_ANIMS_COUNT and make sure both def objects have the same order arrays");

SERIALIZABLE_SIMPLE(AnimMachineDefFileData,
    make_field(o.mRigName, "rig"sv),
    make_field(o.mWalkLeftName, "walk_left"sv),
    make_field(o.mWalkRightName, "walk_right"sv),
    make_field(o.mWalkFrontName, "walk_front"sv),
    make_field(o.mWalkBackName, "walk_back"sv),
    make_field(o.mRunLeftName, "run_left"sv),
    make_field(o.mRunRightName, "run_right"sv),
    make_field(o.mRunFrontName, "run_front"sv),
    make_field(o.mRunBackName, "run_back"sv),
    make_field(o.mSprintFrontName, "sprint_front"sv),
    make_field(o.mIdleName, "idle"sv),
    make_field(o.mIdleCombatName, "idle_combat"sv),
    make_field(o.mFallingName, "fall"sv),
    make_field(o.mJumpName, "jump"sv),
    make_field(o.mLandingName, "land"sv)
);


// TEST ANIM MACHINE
// States
// idle:
//   type: anim_sequence
//   transitions: 
//   - condition: "input.move" == 0
//     transition_duration: 0.2
//     transition_sequence: idle_to_walk
//     speed_curve: CURVE_LINEAR (TODO: Custom Curves)
//     TODO: extract root motion?
//     to_state: walk
// walk:
//   type: blendspace2d
//   transitions:
//   - condition: "input.move" == 0
//     transition_duration: 0.2
//     to_state: idle
//   - condition: "input.move" > 0
//     transition_duration: 0.2
//     to_state: walk
//   - condition: "input.move" < 0
//     transition_duration: 0.2
//     to_state: walk
