#pragma once

#include "ecs/factory/EntityType.h"

// TODO: Entity, not actor
class HumanActorFactory;
class UndeadActorFactory;
class PlayerActorFactory;
class EntityComponentSystem;
class ResourceManager;


class EntityFactory
{
public:
    EntityFactory(EntityComponentSystem& ecs, ResourceManager& resourceManager);
    ~EntityFactory();

    entt::entity createEntity(const f32v2& position, EntityType type);

private:
    std::unique_ptr<HumanActorFactory> mHumanActorFactory;
    std::unique_ptr<UndeadActorFactory> mUndeadActorFactory;
    std::unique_ptr<PlayerActorFactory> mPlayerActorFactory;
};

