#pragma once
#include "character/CharacterLocomotionMode.h"

// Bindings in AnimVariableFloatBinding.h
struct AnimVariables {
    //f32v2 acceleration2d = {};
    f32v2 velocity2d = {};
    f32 speed = {};
    CharacterLocomotionMode locomotionMode = CharacterLocomotionMode::IDLE;
};
