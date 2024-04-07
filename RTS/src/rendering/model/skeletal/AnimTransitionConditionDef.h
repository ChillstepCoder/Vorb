#pragma once

/*
 * Collection of generic useful animation transition conditions. We do not
 * script animation transition conditions, they must be made in c++ here
 */

#include "AnimTransitionCondition.h"

#include <ryml.hpp>

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

typedef std::variant<f32, f32v2> AnimTransitionConditionParamVar;

namespace c4 {
    namespace yml {
        YML_WRITE_DEF(AnimTransitionConditionParamVar) {
            if (std::holds_alternative<f32>(o)) {
                *n << std::get<f32>(o);
            }
            else if (std::holds_alternative<f32v2>(o)) {
                n->operator<<(std::get<f32v2>(o));
            }
        }
        YML_READ_DEF(AnimTransitionConditionParamVar) {
            // TODO: Wont work for other types
            if (n.is_seq()) {
                if (n.num_children() == 2) {
                    f32v2 tmp;
                    n >> tmp;
                    *target = tmp;
                }
                else {
                    return false;
                }
            }
            else {
                f32 tmp;
                n >> tmp;
                *target = tmp;
            }
            return true;
        }
    }
}

struct AnimTransitionConditionFileData {
    AnimTransitionConditionDefType defType;
    // Param options
    AnimTransitionConditionParamVar param;
    static_assert(e_count(AnimTransitionConditionConstantType) == 3);
};
SERIALIZABLE_IMGUI_CONTROLLED(AnimTransitionConditionFileData,
    make_field(o.defType, "type"sv),
    make_field(o.param, "param"sv)
);

extern AnimTransitionConditionDef getAnimTransitionConditionDef(AnimTransitionConditionDefType name);
