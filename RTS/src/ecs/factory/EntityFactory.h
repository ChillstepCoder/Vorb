#pragma once

#include "ecs/factory/EntityType.h"

class IEntityComponentSystem;
class ResourceManager;
class IWorld;

class EntityDefinitionRepository;

// Static class used by EntityComponentSystem to add entities
class EntityFactory
{
    friend class SrvEntityComponentSystem;
    friend class CliEntityComponentSystem;
private:
    static entt::entity createEntity(IWorld& world, const f32v3& position, StrToken typeToken);
};

