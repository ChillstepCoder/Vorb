#include "stdafx.h"
#include "ProjectileComponent.h"

#include "ecs/component/PositionComponent.h"

#include <boost/container/small_vector.hpp>

#include "world/World.h"
#include "world/IHeightmapGrid.h"

constexpr f32 DRAG_FORCE = 0.4f;

enum class ImpactResult {
    NONE,
    GROUND,
    ENTITY,
    TILE,
    OUT_OF_BOUNDS,
    COUNT
};

std::underlying_type<ProjectileFlags>::type REMOVE_FLAGS = e_cast(ProjectileFlags::RemoveOnHit) | e_cast(ProjectileFlags::RemoveOnLand);

// Return  true on impact
ImpactResult updateProjectile(World& world, ProjectileComponent& projCmp, PositionComponent& posCmp, f32 elapsedSec) {
    projCmp.velocity.z += GRAVITY_Z * elapsedSec;
    projCmp.velocity *= MathUtil::dragForceWithDeltaTime(DRAG_FORCE, elapsedSec);
    posCmp.mPosition += projCmp.velocity * elapsedSec;

    const IHeightmapGrid& grid = world.getHeightmapGrid();
    f32 terrainHeight;
    if (grid.tryComputeHeightAtPoint(posCmp.mPosition, &terrainHeight)) {
        if (terrainHeight >= posCmp.mPosition.z) {
            posCmp.mPosition.z = terrainHeight;
            return ImpactResult::GROUND;
        }
    }
    else {
        return ImpactResult::OUT_OF_BOUNDS;
    }

    return ImpactResult::NONE;
}

void ProjectileSystem::addProjectileComponent(entt::registry& registry, entt::entity entity, f32v3 velocity, BitFlags<ProjectileFlags> flags) {
    registry.emplace<ProjectileComponent>(entity, velocity, flags);
}

void ProjectileSystem::update(World& world, entt::registry& registry, f32 elapsedSec) {
    PROFILE_FUNCTION();

    boost::container::small_vector<entt::entity, 32> projectileComponentsToRemove;
    //  TODO: Try group
    auto view = registry.view<ProjectileComponent, PositionComponent>();
    for (auto entity : view) {
        ProjectileComponent& projCmp = view.get<ProjectileComponent>(entity);
        PositionComponent& posCmp = view.get<PositionComponent>(entity);
        ImpactResult result = updateProjectile(world, projCmp, posCmp, elapsedSec);
        switch (result) {
            case ImpactResult::NONE: [[likely]]
                break;
            case ImpactResult::GROUND:
                if (projCmp.flags.isMaskPartiallySet(REMOVE_FLAGS)) {
                    projectileComponentsToRemove.push_back(entity);
                }
                break;
            case ImpactResult::ENTITY:
                // TODO:
                break;
            case ImpactResult::TILE:
                if (projCmp.flags.isBitSet(ProjectileFlags::RemoveOnHit)) {
                    projectileComponentsToRemove.push_back(entity);
                }
                else {
                    // Bounce? Get impact normal?
                }
                break;
            case ImpactResult::OUT_OF_BOUNDS:
                // TODO: Something else? idk
                projectileComponentsToRemove.push_back(entity);
                break;
        }
        static_assert(e_count(ImpactResult) == 5);
    }

    for (entt::entity entity : projectileComponentsToRemove) {
        registry.remove<ProjectileComponent>(entity);
    }
}
