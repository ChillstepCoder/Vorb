#include "stdafx.h"
#include "ProjectileComponent.h"

#include "ecs/component/PositionComponent.h"

#include <boost/container/small_vector.hpp>

#include "world/World.h"
#include "world/IHeightmapGrid.h"

constexpr f32 DRAG_FORCE = 0.4f;

glm::quat alignToTerrainNormal(const glm::vec3& terrainNormal) {
    const glm::vec3 upVector(0.0f, 0.0f, 1.0f); // Z-axis up vector
    glm::vec3 rotationAxis;
    
    if (terrainNormal.z >= 0.9999f) {
        rotationAxis = glm::vec3(1.0f, 0.0f, 0.0f);
    }
    else {
        rotationAxis = glm::cross(upVector, terrainNormal);
    }

    const float rotationAngle = glm::acos(glm::dot(upVector, terrainNormal));
    return glm::angleAxis(rotationAngle, rotationAxis);
}

std::underlying_type<ProjectileFlags>::type REMOVE_FLAGS = e_cast(ProjectileFlags::RemoveOnHit) | e_cast(ProjectileFlags::RemoveOnLand);

// Return  true on impact
std::pair<ProjectileImpactResult, f32v3> updateProjectile(World& world, ProjectileComponent& projCmp, PositionComponent& posCmp, f32 elapsedSec) {
    projCmp.velocity.z += GRAVITY_Z * elapsedSec;
    projCmp.velocity *= MathUtil::dragForceWithDeltaTime(DRAG_FORCE, elapsedSec);
    posCmp.mPosition += projCmp.velocity * elapsedSec;

    const IHeightmapGrid& grid = world.getHeightmapGrid();
    f32v3 terrainNormal(0.0f);
    f32 terrainHeight = grid.computeHeightAndNormalAtPoint<false>(posCmp.mPosition, &terrainNormal);
    if (terrainHeight != FLT_MAX) {
        if (terrainHeight >= posCmp.mPosition.z) {
            posCmp.mPosition.z = terrainHeight;
            return std::make_pair(ProjectileImpactResult::GROUND, terrainNormal);
        }
    }
    else {
        return std::make_pair(ProjectileImpactResult::OUT_OF_BOUNDS, f32v3(0.0f));
    }

    return std::make_pair(ProjectileImpactResult::NONE, f32v3(0.0f));
}

void ProjectileSystem::addProjectileComponent(entt::registry& registry, entt::entity entity, f32v3 velocity, BitFlags<ProjectileFlags> flags, ProjectileRemovedFunc rFunc) {
    registry.emplace<ProjectileComponent>(entity, velocity, flags, rFunc);
}

void ProjectileSystem::update(World& world, entt::registry& registry, f32 elapsedSec) {
    PROFILE_FUNCTION();

    boost::container::small_vector<entt::entity, 32> projectileComponentsToRemove;
    //  TODO: Try group
    auto view = registry.view<ProjectileComponent, PositionComponent>();
    for (auto entity : view) {
        ProjectileComponent& projCmp = view.get<ProjectileComponent>(entity);
        PositionComponent& posCmp = view.get<PositionComponent>(entity);
        auto [result, terrainNormal] = updateProjectile(world, projCmp, posCmp, elapsedSec);
        switch (result) {
            case ProjectileImpactResult::NONE: [[likely]]
                break;
            case ProjectileImpactResult::GROUND:
                if (projCmp.flags.isMaskPartiallySet(REMOVE_FLAGS)) {
                    projectileComponentsToRemove.push_back(entity);
                    if (projCmp.flags.isBitSet(ProjectileFlags::OrientToTerrainOnLand)) {
                        OrientationComponent& orientCmp = registry.get<OrientationComponent>(entity);
                        orientCmp.mOrientation = alignToTerrainNormal(terrainNormal) * orientCmp.mOrientation;
                    }
                    if (projCmp.onRemoved) {
                        projCmp.onRemoved(world, registry, entity, result);
                    }
                }
                break;
            case ProjectileImpactResult::ENTITY:
                // TODO:
                break;
            case ProjectileImpactResult::TILE:
                if (projCmp.flags.isBitSet(ProjectileFlags::RemoveOnHit)) {
                    projectileComponentsToRemove.push_back(entity);
                    if (projCmp.onRemoved) {
                        projCmp.onRemoved(world, registry, entity, result);
                    }
                }
                else {
                    // Bounce? Get impact normal?
                }
                break;
            case ProjectileImpactResult::OUT_OF_BOUNDS:
                // TODO: Something else? idk
                projectileComponentsToRemove.push_back(entity);
                if (projCmp.onRemoved) {
                    projCmp.onRemoved(world, registry, entity, result);
                }
                break;
        }
        static_assert(e_count(ProjectileImpactResult) == 5);
    }

    for (entt::entity entity : projectileComponentsToRemove) {
        registry.remove<ProjectileComponent>(entity);
    }
}
