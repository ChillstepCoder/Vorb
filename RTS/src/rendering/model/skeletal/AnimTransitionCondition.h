#pragma once

#include "character/CharacterLocomotionMode.h"

//typedef std::variant<f32, f32v2> AnimConstant;

enum class AnimTransitionConditionConstantType : ui8 {
    NONE,
    F32,
    F32V2,
    COUNT
};
struct AnimParam {
    AnimParam() = default;
    AnimParam(f32 f) : f(f) {}
    AnimParam(f32v2 f2) : f2(f2) {}

    union {
        f32 f;
        f32v2 f2;
    };
};
static_assert(e_count(AnimTransitionConditionConstantType) == 3);

// TODO: Move
struct AnimVariables {
    f32v2 acceleration2d = {};
    f32v2 velocity2d = {};
    f32 speed = {};
    f32 acceleration = {};
    CharacterLocomotionMode locomotionMode = CharacterLocomotionMode::IDLE;
};

// Transition checks are just function pointers created in code
typedef bool(*AnimTransitionConditionFunc)(const AnimVariables& variables, OPT AnimParam constant);

struct AnimTransitionCondition {
    AnimTransitionConditionFunc func;
    OPT AnimParam constant;
};
