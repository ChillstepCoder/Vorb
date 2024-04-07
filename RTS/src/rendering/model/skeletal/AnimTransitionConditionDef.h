#pragma once

/*
 * Collection of generic useful animation transition conditions. We do not
 * script animation transition conditions, they must be made in c++ here
 */

#include "AnimTransitionCondition.h"

enum class AnimTransitionConditionDefType : ui8 {
    is_in_air,
    is_on_ground,
    is_moving,
    is_accelerating,
    speed_greater_than,
    COUNT
};
SERIALIZABLE_ENUM_SAME_NAME(AnimTransitionConditionDefType,
    ENUM_FIELD_SIMPLE(AnimTransitionConditionDefType, is_in_air),
    ENUM_FIELD_SIMPLE(AnimTransitionConditionDefType, is_on_ground),
    ENUM_FIELD_SIMPLE(AnimTransitionConditionDefType, is_moving),
    ENUM_FIELD_SIMPLE(AnimTransitionConditionDefType, is_accelerating),
    ENUM_FIELD_SIMPLE(AnimTransitionConditionDefType, speed_greater_than)
);
static_assert(e_count(AnimTransitionConditionDefType) == 5);

// Defined in code
struct AnimTransitionConditionDef {
    AnimTransitionConditionDef() = default;
    AnimTransitionConditionDef(AnimTransitionConditionFunc func, StrToken token)
        : func(func), token(token), param({}), constantType(AnimTransitionConditionConstantType::NONE) {}
    AnimTransitionConditionDef(AnimTransitionConditionFunc func, f32 param, StrToken token)
        : func(func), token(token), param(param), constantType(AnimTransitionConditionConstantType::F32) {}
    AnimTransitionConditionDef(AnimTransitionConditionFunc func, f32v2 param, StrToken token)
        : func(func), token(token), param(param), constantType(AnimTransitionConditionConstantType::F32V2) {}

    AnimTransitionConditionFunc func;
    StrToken token;
    AnimParam param;
    AnimTransitionConditionConstantType constantType;
    AnimTransitionConditionDefType defType;
};

struct AnimTransitionConditionFileData {
    AnimTransitionConditionDefType defType;
    // Param options
    std::variant<f32, f32v2> param;
    static_assert(e_count(AnimTransitionConditionConstantType) == 3);
};
SERIALIZABLE_IMGUI_CONTROLLED(AnimTransitionConditionFileData,
    make_field(o.defType, "type"sv),
    make_field(o.param, "param"sv)
);

extern AnimTransitionConditionDef getAnimTransitionConditionDef(AnimTransitionConditionDefType name);
