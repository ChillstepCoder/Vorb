#pragma once

/*
 * Collection of generic useful animation transition conditions. We do not
 * script animation transition conditions, they must be made in c++ here
 */

#include "AnimTransitionCondition.h"

struct AnimTransitionConditionEditorData {
    AnimTransitionCondition condition;
    StrToken token;
};

namespace AnimTransitionConditions {


    inline bool isInAir(const AnimVariables& vars) {
        return vars.locomotionMode == CharacterLocomotionMode::JUMPING ||
               vars.locomotionMode == CharacterLocomotionMode::FALLING;
    }

    inline bool isOnGround(const AnimVariables& vars) {
        return !isInAir(vars) && vars.locomotionMode != CharacterLocomotionMode::SWIMMING;
    }

    inline bool isMoving(const AnimVariables& vars) {
        return vars.speed > 0.01f;
    }

    // TODO: Probably not valid or accurate on simulated proxies?
    inline bool isAccelerating(const AnimVariables& vars) {
        return vars.acceleration > 0.01f;
    }

    inline AnimTransitionCondition getAnimTransitionCondition(StrToken name) {
        static const std::map<StrToken, AnimTransitionCondition> conditions = {
            {CStrToken("is_in_air"), isInAir},
            {CStrToken("is_on_ground"), isOnGround},
            {CStrToken("is_moving"), isMoving},
            {CStrToken("is_accelerating"), isAccelerating}
        };

        auto it = conditions.find(name);
        if (it == conditions.end()) [[unlikely]] {
            panic("Animation transition condigion {} does not exist", name.toString());
        }
        return it->second;
    }

}