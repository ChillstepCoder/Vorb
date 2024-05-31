#pragma once

enum class ProjectileFlags : ui8 {
    RemoveOnLand = BIT(0), // Remove this component when we land on the ground
    RemoveOnHit = BIT(1), // Remove this component when we hit any collision
    OrientToTerrainOnLand = BIT(2), // Orient to the terrain normal when we land on the ground
};

class World;

enum class ProjectileImpactResult {
    NONE,
    GROUND,
    ENTITY,
    TILE,
    OUT_OF_BOUNDS,
    COUNT
};

typedef void (*ProjectileRemovedFunc)(World& world, entt::registry& registry, entt::entity entity, ProjectileImpactResult result);

class ProjectileComponent {
public:
    ProjectileComponent(f32v3 velocity, BitFlags<ProjectileFlags> flags, ProjectileRemovedFunc rFunc) :
        velocity(velocity),
        flags(flags),
        onRemoved(rFunc)
    {}

    f32v3 velocity;
    BitFlags<ProjectileFlags> flags;
    ProjectileRemovedFunc onRemoved = nullptr;
};

class ProjectileSystem {
public:
    static void addProjectileComponent(entt::registry& registry, entt::entity entity, f32v3 velocity, BitFlags<ProjectileFlags> flags, ProjectileRemovedFunc rFunc = nullptr);
    static void update(World& world, entt::registry& registry, f32 elapsedSec);
};
