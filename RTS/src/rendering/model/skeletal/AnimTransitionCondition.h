#pragma once

#include "character/CharacterLocomotionMode.h"

// TODO: Move
struct AnimVariables {
    f32v2 acceleration2d = {};
    f32v2 velocity2d = {};
    f32 speed = {};
    f32 acceleration = {};
    CharacterLocomotionMode locomotionMode = CharacterLocomotionMode::IDLE;
};

// Transition checks are just function pointers created in code
typedef bool(*AnimTransitionCondition)(const AnimVariables& variables);
