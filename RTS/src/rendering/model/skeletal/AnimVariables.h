#pragma once
#include "character/CharacterLocomotionMode.h"

struct AnimVariables {
    f32v2 acceleration2d = {};
    f32v2 velocity2d = {};
    f32 speedSq = {};
    f32 acceleration = {};
    CharacterLocomotionMode locomotionMode = CharacterLocomotionMode::IDLE;
};
