#include "stdafx.h"
#include "ContactFilter.h"

#include "ecs/EntityComponentSystem.h"

#include <box2d/b2_fixture.h>

ContactFilter::ContactFilter(EntityComponentSystem& ecs) : mEcs(ecs)
{

}

bool ContactFilter::ShouldCollide(b2Fixture* fixtureA, b2Fixture* fixtureB) {
    bool shouldCollide = b2ContactFilter::ShouldCollide(fixtureA, fixtureB);
    if (!shouldCollide) {
        return false;
    }
    // 3D checking
    entt::entity entityA = static_cast<entt::entity>(fixtureA->GetUserData().pointer);
    entt::entity entityB = static_cast<entt::entity>(fixtureB->GetUserData().pointer);
    const PhysicsComponent& physA = mEcs.mRegistry.get<PhysicsComponent>(entityA);
    const PhysicsComponent& physB = mEcs.mRegistry.get<PhysicsComponent>(entityB);

    f32 posDiff = physB.mZPosition - physA.mZPosition;
    if (posDiff > 0.0f) {
        // B is above
        return posDiff <= physA.mCollisionHeight;
    }
    else {
        // B is below
        return posDiff >= -physB.mCollisionHeight;
    }
}
