#pragma once

#include "character/CharacterConst.h"

struct CharacterControlComponentDef {
    float mSpeed = 0.3f;
};
KEG_TYPE_DECL(CharacterControlComponentDef);

constexpr f32 LOCOMOTION_MODE_SPEED_MULTS[e_cast(CharacterLocomotionMode::COUNT)] = {
    0.0f, // IDLE
    0.2f, // WALK
    0.6f, // RUN
    1.0f, // SPRINT
    1.0f, // DODGE
    1.0f, // BEGIN_JUMP
    1.0f, // JUMP
    0.8f, // FALLING
    0.8f, // LANDING
};

constexpr f32 LOCOMOTION_MODE_ACCELERATION_MULTS[e_cast(CharacterLocomotionMode::COUNT)] = {
    0.0f, // IDLE
    0.05f, // WALK
    0.05f, // RUN
    0.05f, // SPRINT
    1.0f, // DODGE
    1.0f, // BEGIN_JUMP
    1.0f, // JUMP
    1.0f, // FALLING
    1.0f, // LANDING
};

// TODO: Pull from the anim machine
constexpr f32 FOOTSTEP_CYCLE_DURATION_SEC[e_cast(CharacterLocomotionMode::COUNT)] = {
    0.9f, // IDLE
    0.9f, // WALK
    0.6f, // RUN
    0.5f, // SPRINT
    0.6f, // DODGE
    0.6f, // BEGIN_JUMP
    0.6f, // JUMP
    0.6f, // FALLING
    0.6f, // LANDING
};
static_assert(e_cast(CharacterLocomotionMode::COUNT) == 9, "Update above tables");


enum class CharacterControlComponentFlags : ui8{
    HIDE_MODEL = BIT(0),
    ORIENT_TO_MOVEMENT = BIT(1),
};

struct CharacterControlComponent {
    // TODO: Compress to f32?
    f32v2 mMoveDirection = f32v2(0.0f);
    f32 mControllerAngle = 0.0f;
    f32 mSpeedRun = 4.167f; // ~15 kmph // TODO: AttributesComponent
    CharacterLocomotionMode mMode = CharacterLocomotionMode::IDLE;
    CharacterLocomotionMode mDesiredMode = CharacterLocomotionMode::IDLE;
    BitFlags<CharacterControlComponentFlags> mFlags = BitFlags<CharacterControlComponentFlags>(CharacterControlComponentFlags::ORIENT_TO_MOVEMENT);

    // TODO: Mask
    bool isInAirState() const { return mMode == CharacterLocomotionMode::BEGIN_JUMP || mMode == CharacterLocomotionMode::JUMPING || mMode == CharacterLocomotionMode::FALLING; }

    f32 getCurrentSpeed() const { return mSpeedRun * LOCOMOTION_MODE_SPEED_MULTS[e_cast(mMode)]; }
    f32 getCurrentAcceleration() const { return LOCOMOTION_MODE_ACCELERATION_MULTS[e_cast(mMode)]; }
};
static_assert(sizeof(CharacterControlComponent) == 20, "Keep small");

class CharacterControlSystem {
public:
    void update(entt::registry& registry);
};