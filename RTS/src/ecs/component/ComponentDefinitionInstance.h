#pragma once

#include "ComponentType.h"

#include "ecs/component/Components.h"

class ComponentDefinitionInstance {
public:
    ComponentDefinitionInstance(ComponentType type) : type(type) {};
    ~ComponentDefinitionInstance() {};

    VORB_NON_COPYABLE_BUT_MOVABLE(ComponentDefinitionInstance);

    ComponentType type;
    std::unique_ptr<ComponentDefBase> componentDef; // Optional data
};
static_assert(e_cast(ComponentType::COUNT) == 12, "Set any needed component file data");

