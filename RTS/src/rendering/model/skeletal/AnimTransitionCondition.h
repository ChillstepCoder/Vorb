#pragma once

#include "AnimVariables.h"

//typedef std::variant<f32, f32v2> AnimConstant;

typedef std::variant<std::monostate, f32, f32v2> AnimParamVar;

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
static_assert(std::variant_size_v<AnimParamVar> == 3);

// TODO: Move


// Transition checks are just function pointers created in code
typedef bool(*AnimTransitionConditionFunc)(const AnimVariables& variables, OPT AnimParam constant);

struct AnimTransitionCondition {

    bool passesCondition(const AnimVariables& variables) const { return func(variables, constant); }

    AnimTransitionConditionFunc func;
    OPT AnimParam constant;
};
