#pragma once

#include "rendering/model/AnimationConst.h"

namespace ozz {
    namespace animation {
        class Animation;
    };
};


// Make sure order and contents of the animation machine name and animation arrays are the same
struct AnimMachineDef {
    ui32 mAnimMachineId;
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
};
static_assert(e_cast(AnimMachineState::COUNT) == 14, "Update AnimMachineDef and FileData below");

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
    nString mSprintFrontName;
    nString mIdleName;
    nString mIdleCombatName;
    nString mFallingName;
    nString mJumpName;
    nString mLandingName;
};
KEG_TYPE_DECL(AnimMachineDefFileData);
static_assert(e_cast(AnimMachineState::COUNT) == 14, "Make sure to update ANIMATION_MACHINE_ANIMS_COUNT and make sure both def objects have the same order arrays");