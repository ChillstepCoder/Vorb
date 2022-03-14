#pragma once

enum class LocomotionMode : ui8 {
    IDLE,
    WALK,
    RUN,
    SPRINT,
    DODGE,
    JUMP,
    COUNT
};

struct LocomotionComponentDef {
    float mSpeed;
};
KEG_TYPE_DECL(LocomotionComponentDef);

constexpr f32 LOCOMOTION_MODE_SPEED_MULTS[e_cast(LocomotionMode::COUNT)] = {
    0.0f, // IDLE
    0.3f, // WALK
    0.6f, // RUN
    1.0f, // SPRINT
    1.0f, // DODGE
    0.6f  // JUMP
};

constexpr f32 LOCOMOTION_MODE_ACCELERATION_MULTS[e_cast(LocomotionMode::COUNT)] = {
    0.0f, // IDLE
    0.6f, // WALK
    1.0f, // RUN
    1.5f, // SPRINT
    1.5f, // DODGE
    1.0f  // JUMP
};

struct LocomotionComponent {
    f32 mSpeedRun = 0.3f;
    f32v2 mDesiredDirection = f32v2(0.0f);
    LocomotionMode mMode;

    f32 getCurrentSpeed() const { return mSpeedRun * LOCOMOTION_MODE_SPEED_MULTS[e_cast(mMode)]; }
    f32 getCurrentAcceleration() const { return LOCOMOTION_MODE_ACCELERATION_MULTS[e_cast(mMode)]; }
};
static_assert(sizeof(LocomotionComponent) == 16, "Keep small");

class LocomotionSystem {
public:
    void update(entt::registry& registry);
};