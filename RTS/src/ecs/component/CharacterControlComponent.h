#pragma once

enum class LocomotionMode : ui8 {
    IDLE,
    WALK,
    RUN,
    SPRINT,
    DODGE,
    BEGIN_JUMP,
    JUMPING,
    FALLING,
    LANDING,
    COUNT
};

struct LocomotionComponentDef {
    float mSpeed;
};
KEG_TYPE_DECL(LocomotionComponentDef);

constexpr f32 LOCOMOTION_MODE_SPEED_MULTS[e_cast(LocomotionMode::COUNT)] = {
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

constexpr f32 LOCOMOTION_MODE_ACCELERATION_MULTS[e_cast(LocomotionMode::COUNT)] = {
    0.0f, // IDLE
    0.6f, // WALK
    1.0f, // RUN
    1.5f, // SPRINT
    1.5f, // DODGE
    1.5f, // BEGIN_JUMP
    1.5f, // JUMP
    1.2f, // FALLING
    1.2f, // LANDING
};

// TODO: Pull from the anim machine
constexpr f32 FOOTSTEP_CYCLE_DURATION_SEC[e_cast(LocomotionMode::COUNT)] = {
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
static_assert(e_cast(LocomotionMode::COUNT) == 9, "Update above tables");


struct CharacterControlComponent {
    f32 mSpeedRun = 0.3f;
    f32v2 mDesiredDirection = f32v2(0.0f);
    LocomotionMode mMode = LocomotionMode::IDLE;
    LocomotionMode mDesiredMode = LocomotionMode::IDLE;
    PreciseTimer mLandingTimer; // TODO: This is wrong as it doesn't account tick rate or timestep

    // TODO: Mask
    bool isInAirState() const { return mMode == LocomotionMode::BEGIN_JUMP || mMode == LocomotionMode::JUMPING || mMode == LocomotionMode::FALLING; }

    f32 getCurrentSpeed() const { return mSpeedRun * LOCOMOTION_MODE_SPEED_MULTS[e_cast(mMode)]; }
    f32 getCurrentAcceleration() const { return LOCOMOTION_MODE_ACCELERATION_MULTS[e_cast(mMode)]; }
};
static_assert(sizeof(CharacterControlComponent) == 24, "Keep small");


class LocomotionSystem {
public:
    void update(entt::registry& registry);
};