#pragma once

#include "ecs/factory/EntityType.h"

class EntityComponentSystem;
class ResourceManager;

class EntityDefinitionRepository;
class PhysicsWorld;


class EntityFactory
{
public:
    EntityFactory(EntityComponentSystem& ecs);
    ~EntityFactory();

    entt::entity createEntity(PhysicsWorld& physWorld, const f32v3& position, const nString& typeName);

private:
    EntityComponentSystem& mEcs;
};

