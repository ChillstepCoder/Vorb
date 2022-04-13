#pragma once

#include "ecs/factory/EntityType.h"

class EntityComponentSystem;
class ResourceManager;

class EntityDefinitionRepository;


class EntityFactory
{
public:
    EntityFactory(EntityComponentSystem& ecs);
    ~EntityFactory();

    entt::entity createEntity(const f32v2& position, const nString& typeName);

private:
    EntityComponentSystem& mEcs;
};

