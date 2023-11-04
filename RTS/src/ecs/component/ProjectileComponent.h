#pragma once

enum class ProjectileFlags : ui8 {
    RemoveOnLand = BIT(0), // Remove this component when we land on the ground
    RemoveOnHit = BIT(1), // Remove this component when we hit any collision
};

class World;

class ProjectileComponent {
public:
    ProjectileComponent(f32v3 velocity, BitFlags<ProjectileFlags> flags) :
        velocity(velocity),
        flags(flags)
    {}

    f32v3 velocity;
    BitFlags<ProjectileFlags> flags;
};

class ProjectileSystem {
public:
    static void addProjectileComponent(entt::registry& registry, entt::entity entity, f32v3 velocity, BitFlags<ProjectileFlags> flags);
    static void update(World& world, entt::registry& registry, f32 elapsedSec);
};
