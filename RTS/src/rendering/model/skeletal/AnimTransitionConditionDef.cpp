#include "stdafx.h"
#include "AnimTransitionConditionDef.h"

#include <boost/container/flat_map.hpp>

AnimTransitionConditionDef getAnimTransitionConditionDef(AnimTransitionConditionDefType name) {

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
             return vars.speed > 0.01f;
         }),
         CONDITION_DEF(is_accelerating, {
             return vars.acceleration > 0.01f;
         }),
     };
     static_assert(e_count(AnimTransitionConditionDefType) == 4);
     assert(e_count(AnimTransitionConditionDefType) == conditions.size());

     auto it = conditions.find(name);
     assert(it != conditions.end());
     return it->second;
}