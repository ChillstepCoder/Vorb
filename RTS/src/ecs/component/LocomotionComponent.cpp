#include "stdafx.h"
#include "LocomotionComponent.h"

#include "ecs/component/PhysicsComponent.h"

constexpr float ACCELERATION = 0.05f;
constexpr float JUMP_VELOCITY = 0.15f;

KEG_TYPE_DEF_SAME_NAME(LocomotionComponentDef, kt) {
    kt.addValue("speed", keg::Value::basic(offsetof(LocomotionComponentDef, mSpeed), keg::BasicType::F32));
}

inline void updateComponent(LocomotionComponent& motionCmp, PhysicsComponent& physCmp) {

    // If we have no desired motion, do nothing and let physics system add friction
    if (motionCmp.mMode == LocomotionMode::IDLE) {
        return;
    }

    assert(motionCmp.mDesiredDirection.x != 0.0f || motionCmp.mDesiredDirection.y != 0.0f);

    const float acceleration = ACCELERATION;

    float speed = motionCmp.getCurrentSpeed();
    // TODO: assert normalized?
    // Determine angle to our desired direction
    float dotp = glm::dot(motionCmp.mDesiredDirection, physCmp.mDir);
    dotp = glm::clamp(dotp, -1.0f, 1.0f); // Fix any math rounding errors to prevent NAN acos
    const float angleOffset = acos(dotp);
    assert(angleOffset == angleOffset); // nan check

    // Reduce speed for backstep
    const float speedLerp = glm::clamp((angleOffset - M_PI_2f) / M_PI_2f, 0.0f, 1.0f);
    speed *= 1.0f - (speedLerp * 0.5f);

    // Figure out how far off we are from desired velocity
    const f32v2 targetVelocity = motionCmp.mDesiredDirection * speed;
    f32v2 velocityOffset = targetVelocity - physCmp.getLinearVelocity();
    float velocityDist = glm::length(velocityOffset);

    if (velocityDist <= acceleration) {
        // Set to exact velocity
        physCmp.mBody->SetLinearVelocity(reinterpret_cast<const b2Vec2&>(targetVelocity));
    }
    else {
        // Accelerate to the velocity
        //const f32v2& currentLinearVelocity = reinterpret_cast<const f32v2&>(physCmp.mBody->GetLinearVelocity());
        //velocityOffset = (velocityOffset / velocityDist) * acceleration + currentLinearVelocity;
        //physCmp.mBody->SetLinearVelocity(reinterpret_cast<const b2Vec2&>(velocityOffset));
        f32v2 force = motionCmp.mDesiredDirection * acceleration * 0.1f;
        physCmp.mBody->ApplyForceToCenter(reinterpret_cast<const b2Vec2&>(force), true);
    }

    // Jumping
    if (motionCmp.mMode == LocomotionMode::JUMP) {
        physCmp.setZVelocity(JUMP_VELOCITY);
    }
}

void LocomotionSystem::update(entt::registry& registry) {
    // Update components
    // TODO: Check performance of lambda vs non lambda iteration (see PlayerControlComponent)
    registry.view<LocomotionComponent, PhysicsComponent>().each([](auto& motionCmp, auto& physCmp) {
        updateComponent(motionCmp, physCmp);
    });
}
