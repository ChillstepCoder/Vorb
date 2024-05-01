#pragma once


#include "ecs/component/ComponentDefinition.h"

class EntityDef : public IAsset {
public:
    DEFAULT_ASSET_CONSTRUCTOR(EntityDef, AssetType::Entity);

    std::vector<ComponentDefinition> components;
};