#pragma once

#include "character/CharacterLocomotionMode.h"
#include "ecs/component/ComponentDefBase.h"

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Character/CharacterBase.h>

class World;

class CharacterControlComponentDef: public ComponentDefBase {
public:
    float mSpeed = 0.3f;
};
SERIALIZABLE_SIMPLE(CharacterControlComponentDef,
    make_field(o.mSpeed, "speed"sv)
);

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
    0.95f, // WALK
    0.98f, // RUN
    0.99f, // SPRINT
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
    1.0f, // SWIMMING
};
static_assert(e_cast(CharacterLocomotionMode::COUNT) == 10, "Update above tables");


enum class CharacterControlComponentFlags : ui8{
    HideModel = BIT(0),
    OrientToMovement = BIT(1),
    IsPlayerController = BIT(2),
};

struct CharacterControlComponent {
    // Is either a player controller or an AI controller based on the flags
    std::unique_ptr<JPH::CharacterBase> mCharacterController;
    f32v2 mMoveDirection = f32v2(0.0f);
    f32 mControllerAngleRad = 0.0f;
    f32 mSpeedRun = 4.167f; // ~15 kmph // TODO: AttributesComponent
    CharacterLocomotionMode mLocomotionMode = CharacterLocomotionMode::IDLE;
    CharacterLocomotionMode mDesiredLocomotionMode = CharacterLocomotionMode::IDLE;
    BitFlags<CharacterControlComponentFlags> mFlags = BitFlags<CharacterControlComponentFlags>(CharacterControlComponentFlags::OrientToMovement);

    f32v2 getControllerDir() const { return f32v2(cos(mControllerAngleRad), sin(mControllerAngleRad)); }

    // TODO: Mask
    bool isInAirState() const { return mLocomotionMode == CharacterLocomotionMode::BEGIN_JUMP || mLocomotionMode == CharacterLocomotionMode::JUMPING || mLocomotionMode == CharacterLocomotionMode::FALLING; }

    f32 getCurrentMaxSpeed() const { return mSpeedRun * LOCOMOTION_MODE_SPEED_MULTS[e_cast(mLocomotionMode)]; }
    f32 getCurrentAcceleration() const { return LOCOMOTION_MODE_ACCELERATION_MULTS[e_cast(mLocomotionMode)]; }
};
static_assert(sizeof(CharacterControlComponent) == 32, "Keep small");

class CharacterControlSystem {
public:
    void update(World& world, entt::registry& registry, f32 elapsedSec);
};