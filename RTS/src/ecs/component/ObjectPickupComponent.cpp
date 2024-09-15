#include "stdafx.h"
#include "ObjectPickupComponent.h"

#include "ecs/component/PositionComponent.h"
#include "ecs/factory/EntityFactory.h"

#include "world/World.h"
#include "util/MathUtil.hpp"
#include "effect/IEffectContext.h"

constexpr f32 ACCEL_FORCE = 60.0f;
constexpr f32 END_PICKUP_DISTANCE = 0.1f;

void ObjectPickupSystem::update(World& world, entt::registry& registry, f32 elapsedSec) {
    PROFILE_FUNCTION();

    registry.view<ObjectPickupComponent>().each([&](auto entity, ObjectPickupComponent& cmp) {
        bool destroy = false;

        PositionComponent& objectPos = registry.get<PositionComponent>(entity);
        const PositionComponent& pickerPos = registry.get<PositionComponent>(cmp.picker);
        const f32v3 magnetPoint(pickerPos.mPosition.x, pickerPos.mPosition.y, pickerPos.mPosition.z + 0.6f);
        const f32v3 offset = magnetPoint - objectPos.mPosition;
        const f32 lengthSq = glm::length2(offset);
        if (lengthSq <= SQ(END_PICKUP_DISTANCE)) {
            destroy = true;
        }
        else {

            const f32 remainingDistance = sqrtf(lengthSq);
            const f32v3 normal = offset / remainingDistance;
            auto [deltaPos, deltaVel] = MathUtil::accelerateWithDeltaTime<f32v3, f32>(normal * cmp.speed, ACCEL_FORCE, elapsedSec);
            cmp.speed += deltaVel;

            // If we are about to move over end, destroy
            if (glm::length2(deltaVel) >= SQ(remainingDistance)) {
                destroy = true;
            }
            else {
                objectPos.mPosition += deltaPos;
            }

        }
        if (destroy) {
            world.getEffectContext().playParticleEffectAtPoint(EffectAssetRef(CStrToken("item_pickup")), objectPos.mPosition, f32q(), nullptr, BitFlags<EffectCreateFlags>());
            EntityFactory::destroyEntity(world, entity);
        }
    });
}
