#pragma once

#include "item/ItemStack.h"

class IEntityComponentSystem;
class ResourceManager;
class World;

class EntityRepository;

// Static class used by EntityComponentSystem to add entities
class EntityFactory
{
    friend class SrvEntityComponentSystem;
    friend class CliEntityComponentSystem;

public:
    // TODO: Replication doesn't work for this entity type, as it is not driven by the ECS.
    // Perhaps instead the ECS should listen for entity create, and then handle replication?
    static entt::entity createItemProjectile(World& world, f32v3 position, f32v3 velocity, ItemStack itemStack);
    static entt::entity createItemOnGround(World& world, f32v3 position, ItemStack itemStack);

    static void destroyEntity(World& world, entt::entity entity);

private:
    static entt::entity createEntity(World& world, f32v3 position, StrToken typeToken);
};

