#include "stdafx.h"
#include "AnimTransitionConditionDef.h"

#include <boost/container/flat_map.hpp>

constexpr f32 MOVING_THRESHOLD = 0.01f;

const AnimTransitionConditionDef& getAnimTransitionConditionDef(AnimTransitionConditionDefType name) {

#define CONDITION_DEF(token, func) {AnimTransitionConditionDefType::token, AnimTransitionConditionDef{ [](const AnimVariables& vars, AnimParam) func, CStrToken(#token)}}
#define CONDITION_DEF_PARAM(token, defaultParam, func) {AnimTransitionConditionDefType::token, AnimTransitionConditionDef{ [](const AnimVariables& vars, AnimParam p) func, defaultParam, CStrToken(#token)}}

    static const boost::container::flat_map<AnimTransitionConditionDefType, AnimTransitionConditionDef> conditions = {
        CONDITION_DEF(is_in_air, {
            return vars.locomotionMode == CharacterLocomotionMode::JUMPING ||
                    vars.locomotionMode == CharacterLocomotionMode::FALLING;
        }),
        CONDITION_DEF(is_on_ground, {
            return vars.locomotionMode != CharacterLocomotionMode::JUMPING &&
                    vars.locomotionMode != CharacterLocomotionMode::FALLING &&
                    vars.locomotionMode != CharacterLocomotionMode::SWIMMING;
        }),
        CONDITION_DEF(is_moving, {
            return vars.speed > MOVING_THRESHOLD;
        }),
        CONDITION_DEF(is_not_moving, {
            return vars.speed <= MOVING_THRESHOLD;
        }),
        CONDITION_DEF(is_accelerating, {
            panic("NO is_accelerating IMPLEMENTED"); return false;
         }),
         CONDITION_DEF_PARAM(speed_greater_than, 1.0f, {
             return vars.speed > SQ(p.f);
         }),
     };
     static_assert(e_count(AnimTransitionConditionDefType) == 6);
     assert(e_count(AnimTransitionConditionDefType) == conditions.size());

     auto it = conditions.find(name);
     assert(it != conditions.end());
     return it->second;
}