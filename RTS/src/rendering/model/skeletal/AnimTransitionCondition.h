#pragma once

#include "character/CharacterLocomotionMode.h"

//typedef std::variant<f32, f32v2> AnimConstant;

typedef std::variant<f32, f32v2> AnimParamVar;

// Efficient version of AnimTransitionConditionParamVar which allows the condition to make an assumption on type
struct AnimParam {
    AnimParam() = default;
    AnimParam(f32 f) : f(f) {}
    AnimParam(f32v2 f2) : f2(f2) {}

    union {
        f32 f;
        f32v2 f2;
    };
};
static_assert(std::variant_size_v<AnimParamVar> == 2);

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
