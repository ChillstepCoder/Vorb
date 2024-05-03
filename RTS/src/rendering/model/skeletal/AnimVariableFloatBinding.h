#pragma once

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
    f32v2 range = f32v2(0.0f, 1.0f); // If constant, will use range.x
};
SERIALIZABLE_IMGUI_CONTROLLED(AnimVariableFloatBindingDef,
    make_field(o.bindingType, "binding"sv),
    make_field(o.range, "range"sv)
);

struct AnimVariableFloatBinding {
    int byteOffset = INT32_MAX; // Offset into AnimVariables
    f32 rangeStart = 0.0f; // Runtime uses inverseRange to scale and does not need range.y
    f32 inverseRange = 1.0f; // 1 / (range.y - range.x)
};

