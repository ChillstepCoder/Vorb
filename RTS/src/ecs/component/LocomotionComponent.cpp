#include "stdafx.h"
#include "LocomotionComponent.h"

#include "options/DebugOptions.h"

#include "ecs/component/PhysicsComponent.h"

constexpr float ACCELERATION = 0.01f;
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

    float desiredSpeed = motionCmp.getCurrentSpeed();
    // TODO: assert normalized?
    // Determine angle to our desired direction
    float dotp = glm::dot(motionCmp.mDesiredDirection, physCmp.mDir);
    dotp = glm::clamp(dotp, -1.0f, 1.0f); // Fix any math rounding errors to prevent NAN acos
    const float angleOffset = acos(dotp);
    assert(angleOffset == angleOffset); // nan check

    // Reduce speed for backstep
    const float speedLerp = glm::clamp((angleOffset - M_PI_2f) / M_PI_2f, 0.0f, 1.0f);
    desiredSpeed *= 1.0f - (speedLerp * 0.5f);

    // Figure out how far off we are from desired velocity
    const f32 currentSpeed = glm::length(physCmp.getLinearVelocity());

    // F = M * A
    // A = dv/dt
    // Linear damping useful info: https://gamedev.stackexchange.com/questions/160047/what-does-lineardamping-mean-in-box2d
    const f32 mass = physCmp.mBody->GetMass();
    const f32 currentSpeedInDirection = currentSpeed * dotp;
    const f32 damping = 1.0f - physCmp.mBody->GetLinearDamping();
    desiredSpeed /= damping;
    if (currentSpeedInDirection < desiredSpeed) {

        const f32 additionalSpeedNeeded = desiredSpeed - currentSpeedInDirection;
        const f32 forceToDesiredSpeed = (additionalSpeedNeeded * mass);
        const f32 forceToApply = glm::min(forceToDesiredSpeed, motionCmp.getCurrentAcceleration() * ACCELERATION);

        const f32v2 force = motionCmp.mDesiredDirection * forceToApply;
        physCmp.mBody->ApplyForceToCenter(reinterpret_cast<const b2Vec2&>(force), true);
        /* std::cout << "APPLYING FORCE " << force.x << " " << force.y << " " << glm::length(force) << "\n";
         std::cout << "  CURRENT SPEED " << currentSpeed << " vs desired " << desiredSpeed * damping << "\n";
         std::cout << "  FORCE NEEDED " << forceToDesiredSpeed << " " << forceToApply << std::endl;*/
    }

    //}

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
