#pragma once
#include "character/CharacterLocomotionMode.h"

enum class AnimVariableFloatBindingType : ui8 {
    Constant,
    //AccelerationX,
    //AccelerationY,
    VelocityX,
    VelocityY,
    Speed,
   // Acceleration
    COUNT
};
SERIALIZABLE_ENUM_SAME_NAME(AnimVariableFloatBindingType,
    ENUM_FIELD_SIMPLE(AnimVariableFloatBindingType, Constant),
    ENUM_FIELD_SIMPLE(AnimVariableFloatBindingType, VelocityX),
    ENUM_FIELD_SIMPLE(AnimVariableFloatBindingType, VelocityY),
    ENUM_FIELD_SIMPLE(AnimVariableFloatBindingType, Speed),
);
static_assert(e_count(AnimVariableFloatBindingType) == 4);

struct AnimVariableFloatBindingDef {
    AnimVariableFloatBindingType bindingType = AnimVariableFloatBindingType::COUNT;
    f32 scale = 1.0f;
    f32 offset = 0.0f;
};
SERIALIZABLE_IMGUI_CONTROLLED(AnimVariableFloatBindingDef,
    make_field(o.bindingType, "binding"sv),
    make_field(o.scale, "scale"sv),
    make_field(o.offset, "offset"sv)
);

struct AnimVariableFloatBinding {
    int byteOffset = INT32_MAX; // Offset into AnimVariables
    f32 scale = 1.0f;
    f32 offset = 0.0f;
};

struct AnimVariables {
    //f32v2 acceleration2d = {};
    f32v2 velocity2d = {};
    f32 speed = {};
    CharacterLocomotionMode locomotionMode = CharacterLocomotionMode::IDLE;
};
