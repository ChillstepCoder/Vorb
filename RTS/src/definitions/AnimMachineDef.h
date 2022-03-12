#pragma once

namespace ozz {
    namespace animation {
        class Animation;
    };
};

// Keep this up to date as new animations are added
const ui32 ANIMATION_MACHINE_ANIMS_COUNT = 10u;

// Make sure this matches AnimMachineDef
enum class AnimMachineState : ui16 {
    WALK_LEFT,
    WALK_RIGHT,
    WALK_FRONT,
    WALK_BACK,
    RUN_LEFT,
    RUN_RIGHT,
    RUN_FRONT,
    RUN_BACK,
    IDLE,
    IDLE_COMBAT,
    COUNT
};

// Make sure order and contents of the animation machine name and animation arrays are the same
struct AnimMachineDef {
    ui32 mAnimMachineId;
    union {
        struct {
            ozz::animation::Animation* mWalkLeftAnim;
            ozz::animation::Animation* mWalkRightAnim;
            ozz::animation::Animation* mWalkFrontAnim;
            ozz::animation::Animation* mWalkBackAnim;
            ozz::animation::Animation* mRunLeftAnim;
            ozz::animation::Animation* mRunRightAnim;
            ozz::animation::Animation* mRunFrontAnim;
            ozz::animation::Animation* mRunBackAnim;
            ozz::animation::Animation* mIdleAnim;
            ozz::animation::Animation* mIdleCombatAnim;
        };
        ozz::animation::Animation* mAnimsArray[ANIMATION_MACHINE_ANIMS_COUNT] = {};
    };
};
static_assert(e_cast(AnimMachineState::COUNT) == 10, "Update AnimMachineDef and FileData below");

struct AnimMachineDefFileData {
    nString mRigName;

    // We will iterate these like an array, make sure they are correct
    nString mWalkLeftName; //< This must remain the first element in the list of animation, see AnimationMachineRepository.cpp (loadMachineFile)
    nString mWalkRightName;
    nString mWalkFrontName;
    nString mWalkBackName;
    nString mRunLeftName;
    nString mRunRightName;
    nString mRunFrontName;
    nString mRunBackName;
    nString mIdleName;
    nString mIdleCombatName;
};
KEG_TYPE_DECL(AnimMachineDefFileData);
static_assert(sizeof(AnimMachineDefFileData) == 440, "Make sure to update ANIMATION_MACHINE_ANIMS_COUNT and make sure both def objects have the same order arrays");