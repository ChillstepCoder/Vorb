#include "stdafx.h"
#include "CameraAttachComponent.h"

#include "ecs/IEntityComponentSystem.h"

//
//void updateComponent(IWorld& world, entt::registry& registry, entt::entity entity, CameraAttachComponent& cameraCmp, PhysicsComponent& physics) {
//    Camera3D& camera = cameraCmp.mCamera;
//    camera.update();
//}
//
//void CameraAttachSystem::update(IWorld& world, entt::registry& registry) {
//    PROFILE_FUNCTION();
//    auto view = registry.view<CameraAttachComponent, PhysicsComponent>();
//    for (auto entity : view) {
//        CameraAttachComponent& cameraCmp = view.get<CameraAttachComponent>(entity);
//        PhysicsComponent& physics = view.get<PhysicsComponent>(entity);
//        updateComponent(world, registry, entity, cameraCmp, physics);
//    }
//}
