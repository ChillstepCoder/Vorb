#pragma once

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
    SPRINT_FRONT,
    IDLE,
    IDLE_COMBAT,
    FALLING,
    JUMPING,
    LANDING,
    COUNT
};

const ui32 ANIMATION_MACHINE_ANIMS_COUNT = e_cast(AnimMachineState::COUNT);

constexpr const char* AnimMachineStateNames[e_cast(AnimMachineState::COUNT)] = {
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

static_assert(e_cast(AnimMachineState::COUNT) == 14, "Update debug strings");

typedef ui32 AnimationID;