#pragma once

#include "ecs/factory/EntityType.h"
#include "util/StrToken.h"

class IEntityComponentSystem;
class ResourceManager;

class EntityDefinitionRepository;

// Static class used by EntityComponentSystem to add entities
class EntityFactory
{
    friend class SrvEntityComponentSystem;
    friend class CliEntityComponentSystem;
private:
    static entt::entity createEntity(const f32v3& position, StrToken typeToken);
};

