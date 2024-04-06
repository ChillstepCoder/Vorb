#pragma once


// Forward declare animations
namespace ozz {
    namespace animation {
        class Animation;
    };
};

using Animation = ozz::animation::Animation;

// Make sure this matches AnimMachineDef
enum class AnimMachineStateOLD : ui16 {
    WALK_LEFT,
    WALK_RIGHT,
    WALK_FRONT,
    WALK_BACK,
    RUN_LEFT,
    RUN_RIGHT,
    RUN_FRONT,
    RUN_BACK,
    SPRINT_FRONT,
    IDLE,
    IDLE_COMBAT,
    FALLING,
    JUMPING,
    LANDING,
    COUNT
};

constexpr ui32 NUM_ANIM_STATE_TRACKS = e_cast(AnimMachineStateOLD::COUNT);
constexpr ui32 ANIMATION_MACHINE_ANIMS_COUNT = e_cast(AnimMachineStateOLD::COUNT);

constexpr const char* AnimMachineStateNames[e_cast(AnimMachineStateOLD::COUNT)] = {
    "WALK_LEFT",
    "WALK_RIGHT",
    "WALK_FRONT",
    "WALK_BACK",
    "RUN_LEFT",
    "RUN_RIGHT",
    "RUN_FRONT",
    "RUN_BACK",
    "SPRINT_FRONT",
    "IDLE",
    "IDLE_COMBAT",
    "FALLING",
    "JUMPING",
    "LANDING"
};

static_assert(e_cast(AnimMachineStateOLD::COUNT) == 14, "Update debug strings");
