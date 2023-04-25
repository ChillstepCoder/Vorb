#pragma once

#include "ComponentTypes.h"

// All serializable component includes
#include "ecs/component/PlayerControlComponent.h"
#include "ecs/component/CharacterControlComponent.h"
#include "ecs/component/CombatComponent.h"
#include "ecs/component/CorpseComponent.h"
#include "ecs/component/CharacterDetailsComponent.h"
#include "ecs/component/DynamicLightComponent.h"
#include "ecs/component/NavigationComponent.h"
#include "ecs/component/PersonAIComponent.h"
#include "ecs/component/PhysicsComponent.h"
#include "ecs/component/ProfessionComponent.h"
#include "ecs/component/SimpleSpriteComponent.h"
#include "ecs/component/SoldierAIComponent.h"
#include "ecs/component/UndeadAIComponent.h"
#include "ecs/component/TimedTileInteractComponent.h"
#include "ecs/component/InventoryComponent.h"
#include "ecs/component/SkillsComponent.h"
#include "ecs/business/BusinessComponent.h"
// Charactermodel has a component TODO: Split
#include "rendering/CharacterModel.h"
static_assert(e_cast(ComponentTypes::COUNT) == 15, "Update component includes");

struct ComponentDefinition {
    ComponentDefinition(ComponentTypes type) : type(type) {};
    ~ComponentDefinition() {};

    // Union based on type
    ComponentTypes type;
    union {
        PhysicsComponentDef          physics;
        SimpleSpriteComponentDef     simpleSprite;
        CharacterDetailsComponentDef characterDetails;
        CharacterControlComponentDef characterControl;
    };
    // TODO: Instead of union, polymorphism? This doesnt work in union due to array destructor
    SkillsComponentFileData      skillsFileData;
};
static_assert(e_cast(ComponentTypes::COUNT) == 15, "Set any needed component file data");