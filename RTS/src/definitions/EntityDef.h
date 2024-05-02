#pragma once


#include "ecs/component/ComponentDefinitionInstance.h"

class EntityDef : public IAsset {
public:
    DEFAULT_ASSET_CONSTRUCTOR(EntityDef, AssetType::Entity);

    std::vector<ComponentDefinitionInstance> components;
};