#pragma once

#include "ComponentTypes.h"

#include "ecs/component/Components.h"

struct ComponentDefinition {
    ComponentDefinition(ComponentTypes type) : type(type) {};
    ~ComponentDefinition() {};

    // Union based on type
    ComponentTypes type;
    x; // Turn these into unique_ptr
    union {
        PhysicsComponentDef          physics;
        CharacterDetailsComponentDef characterDetails;
        CharacterControlComponentDef characterControl;
        CharacterModelComponentDef   characterModel;
        SkillsComponentFileData      skillsFileData;
    };
};
static_assert(e_cast(ComponentTypes::COUNT) == 12, "Set any needed component file data");

