#pragma once

#include "ComponentTypes.h"

// All component includes
#include "ecs/component/PlayerControlComponent.h"
#include "ecs/component/CombatComponent.h"
#include "ecs/component/CorpseComponent.h"
#include "ecs/component/DynamicLightComponent.h"
#include "ecs/component/NavigationComponent.h"
#include "ecs/component/PhysicsComponent.h"
#include "ecs/component/CorpseComponent.h"
#include "ecs/component/ProfessionComponent.h"
#include "ecs/component/SimpleSpriteComponent.h"
#include "ecs/component/SoldierAIComponent.h"
#include "ecs/component/UndeadAIComponent.h"

struct ComponentDefinition {
    ComponentDefinition(ComponentTypes type) : type(type) {};
    ~ComponentDefinition() {};

    // Union based on type
    ComponentTypes type;
    union {
        PhysicsComponentDef      physics;
        SimpleSpriteComponentDef simpleSprite;
    };
};
static_assert(enum_cast(ComponentTypes::COUNT) == 11, "Set any needed component def");