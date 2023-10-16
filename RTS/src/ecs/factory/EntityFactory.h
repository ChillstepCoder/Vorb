#pragma once

#include "ecs/factory/EntityType.h"

class IEntityComponentSystem;
class ResourceManager;
class World;

class EntityDefinitionRepository;

// Static class used by EntityComponentSystem to add entities
class EntityFactory
{
    friend class SrvEntityComponentSystem;
    friend class CliEntityComponentSystem;
private:
    static entt::entity createEntity(World& world, const f32v3& position, StrToken typeToken);
};

