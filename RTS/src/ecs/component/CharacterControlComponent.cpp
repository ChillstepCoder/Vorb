#include "stdafx.h"
#include "CharacterControlComponent.h"

#include "options/DebugOptions.h"

#include "ecs/component/PhysicsComponent.h"
#include "physics/DynamicCharacterController.h"
#include "physics/PhysicsConst.h"

constexpr float JUMP_VELOCITY = 4.0f;

KEG_TYPE_DEF_SAME_NAME(CharacterControlComponentDef, kt) {
    kt.addValue("speed", keg::Value::basic(offsetof(CharacterControlComponentDef, mSpeed), keg::BasicType::F32));
}

inline float interpolateYaw(float currentYaw, float targetYaw, float speed) {
    float yawDifference = targetYaw - currentYaw;

    // Normalize the yaw difference to the range of -180 to 180 degrees
    if (yawDifference > M_PIF) {
        yawDifference -= M_2_PIF;
    }
    else if (yawDifference < -M_PIF) {
        yawDifference += M_2_PIF;
    }

    // Calculate the interpolation step
    float interpolationStep = speed;

    // Clamp the step to not overshoot the target
    const float absYawDif = glm::abs(yawDifference);
    if (absYawDif < interpolationStep) {
        interpolationStep = absYawDif;
    }

    // Apply the interpolation step in the correct direction
    if (yawDifference > 0.0f) {
        currentYaw += interpolationStep;
    }
    else {
        currentYaw -= interpolationStep;
    }

    //// Normalize the result to the range of 0 to 360 degrees
    //while (currentYaw < 0.0f) currentYaw += M_2_PIF;
    //while (currentYaw > M_2_PIF) currentYaw -= M_2_PIF;

    return currentYaw;
}

inline void updateComponent(CharacterControlComponent& controlCmp, PhysicsComponent& physCmp) {
    //DynamicCharacterController& controller = *controlCmp.mController;
    // Transitions
    btRigidBody* rigidBody = physCmp.mRigidBody;
    const bool onGround = physCmp.mFlags.isBitSet(PhysicsComponentFlag::IS_ON_GROUND);

    const btVector3& currentLinearVelocity = rigidBody->getLinearVelocity();
    if (controlCmp.mDesiredMode == CharacterLocomotionMode::BEGIN_JUMP && onGround) {
        controlCmp.mDesiredMode = CharacterLocomotionMode::JUMPING;
        controlCmp.mMode = CharacterLocomotionMode::JUMPING;
        // Immediately adjust linear velocity to account the jump, this will update currentLinearVelocity
        btVector3 jumpVelocity = btVector3(currentLinearVelocity.x(), currentLinearVelocity.y(), JUMP_VELOCITY);
        rigidBody->setLinearVelocity(jumpVelocity);
    }
    else if (controlCmp.isInAirState()) {
        if (onGround) {
            // Transition back to grounded
            controlCmp.mMode = CharacterLocomotionMode::LANDING;
            //controlCmp.mLandingTimer.start();
        }
        else if (controlCmp.mMode == CharacterLocomotionMode::JUMPING) {
            if (physCmp.mRigidBody->getLinearVelocity().getZ() <= 0.0f) {
                controlCmp.mMode = CharacterLocomotionMode::FALLING;
            }
        }
    }

    if (controlCmp.mMode != controlCmp.mDesiredMode) {
        if (controlCmp.mMode == CharacterLocomotionMode::LANDING) {
            // TODO: HMM IM NOT SURE ABOUT THISSSSS
           // constexpr f32 LANDING_ANIM_DURATION_MS = 200.0f;
           // if (controlCmp.mLandingTimer.stop() >= LANDING_ANIM_DURATION_MS) {
                controlCmp.mMode = controlCmp.mDesiredMode;
           // }
        }
        else {
            controlCmp.mMode = controlCmp.mDesiredMode;
        }
    }

    const bool isTryingToMove = (controlCmp.mMoveDirection.x != 0.0f || controlCmp.mMoveDirection.y != 0.0f);
    const f32 currentMaxSpeed = controlCmp.getCurrentSpeed();
    const f32 currentAcceleration = controlCmp.getCurrentAcceleration();

    const f32v2 currentLinearVelocity2D(currentLinearVelocity.x(), currentLinearVelocity.y());
    f32v2 newLinearVelocity2D;
    if (isTryingToMove) {
        f32v2 desiredLinearVelocity2D;
        if (onGround || currentLinearVelocity.z() > 0.0f) {
            // If we are on the ground or moving upwards, allow control
            desiredLinearVelocity2D = f32v2(controlCmp.mMoveDirection.x * currentMaxSpeed, controlCmp.mMoveDirection.y * currentMaxSpeed);
        }
        else {
            // Allow limited control while falling
            constexpr f32 FALL_CONTROL_MULT = 1.0f;
            desiredLinearVelocity2D = f32v2(controlCmp.mMoveDirection.x * FALL_CONTROL_MULT * currentMaxSpeed, controlCmp.mMoveDirection.y * FALL_CONTROL_MULT * currentMaxSpeed);
        }
        // https://www.construct.net/en/blogs/ashleys-blog-2/using-lerp-delta-time-924
        newLinearVelocity2D = vmath::lerp(currentLinearVelocity2D, desiredLinearVelocity2D, currentAcceleration);
    }
    else {
        // Friction
        constexpr f32 GROUND_FRICTION = 0.8f;
        constexpr f32 AIR_FRICTION = 0.2f;
        if (onGround) {
            newLinearVelocity2D = vmath::lerp(currentLinearVelocity2D, f32v2(0.0f), GROUND_FRICTION);
        }
        else {
            newLinearVelocity2D = vmath::lerp(currentLinearVelocity2D, f32v2(0.0f), AIR_FRICTION);
        }
        // NOTE: If we ever need delta time https://www.construct.net/en/blogs/ashleys-blog-2/using-lerp-delta-time-924
    }

    // Use lerp instead of force so that we dont orbit
    // https://www.construct.net/en/blogs/ashleys-blog-2/using-lerp-delta-time-924
    //linearVelocity = linearVelocity.lerp(desiredVelocity, 1.0f - currentAcceleration);


    //if (onGround) {
    //    /* Avoid going down on ramps, if already on ground, and clearGravity()
    //    is not enough */
    //    rigidBody->setGravity({ 0, 0, 0 });
    //}
    //else {
    //    rigidBody->setGravity(mGravity);
    //}
    const btVector3 newLinearVelocity(newLinearVelocity2D.x, newLinearVelocity2D.y, currentLinearVelocity.z());
    rigidBody->setLinearVelocity(newLinearVelocity);
    rigidBody->activate(true); // FORCE

    // Orient rotation to movement
    if (controlCmp.mFlags.isBitSet(CharacterControlComponentFlags::ORIENT_TO_MOVEMENT)) {
        constexpr f32 YAW_SPEED = 0.12f;
        controlCmp.mControllerAngle = interpolateYaw(controlCmp.mControllerAngle, atan2(newLinearVelocity2D.y, newLinearVelocity2D.x), YAW_SPEED);
    }

    // If we have no desired motion, do nothing and let physics system add friction
   /* if (motionCmp.mDesiredDirection.x == 0.0f && motionCmp.mDesiredDirection.y == 0.0f) {
        return;
    }*/

    //// TODO: assert normalized?
    //// Determine angle to our desired direction
    //float dotp = glm::dot(motionCmp.mDesiredDirection, physCmp.getDir());
    //dotp = glm::clamp(dotp, -1.0f, 1.0f); // Fix any math rounding errors to prevent NAN acos
    //const float angleOffset = acos(dotp);
    //assert(angleOffset == angleOffset && "Nan angle offset"); // nan check

    //// Reduce speed for backstep
    //const float speedLerp = glm::clamp((angleOffset - M_PI_2f) / M_PI_2f, 0.0f, 1.0f);
    //desiredSpeed *= 1.0f - (speedLerp * 0.5f);

    //// Figure out how far off we are from desired velocity
    //const f32 currentSpeed = glm::length(physCmp.getLinearVelocity());

    //// F = M * A
    //// A = dv/dt
    //// Linear damping useful info: https://gamedev.stackexchange.com/questions/160047/what-does-lineardamping-mean-in-box2d
    //const f32 linearDamping = 0.1f;
    //const f32 mass = 1.0f;
    //const f32 currentSpeedInDirection = currentSpeed * dotp;
    //const f32 damping = 1.0f - linearDamping;
    //desiredSpeed /= damping;
    //if (currentSpeedInDirection < desiredSpeed) {

    //    const f32 additionalSpeedNeeded = desiredSpeed - currentSpeedInDirection;
    //    const f32 forceToDesiredSpeed = (additionalSpeedNeeded * mass);
    //    const f32 forceToApply = glm::min(forceToDesiredSpeed, motionCmp.getCurrentAcceleration() * ACCELERATION);

    //    const f32v2 force = motionCmp.mDesiredDirection * forceToApply;
    //    //physCmp.mBody->ApplyForceToCenter(reinterpret_cast<const b2Vec2&>(force), true);
    //    /* std::cout << "APPLYING FORCE " << force.x << " " << force.y << " " << glm::length(force) << "\n";
    //     std::cout << "  CURRENT SPEED " << currentSpeed << " vs desired " << desiredSpeed * damping << "\n";
    //     std::cout << "  FORCE NEEDED " << forceToDesiredSpeed << " " << forceToApply << std::endl;*/
    //}

    //}

}

void CharacterControlSystem::update(entt::registry& registry) {
    PROFILE_FUNCTION();
    // Update components
    // TODO: Check performance of lambda vs non lambda iteration (see PlayerControlComponent)
    registry.view<CharacterControlComponent, PhysicsComponent>().each([](auto& motionCmp, auto& physCmp) {
        updateComponent(motionCmp, physCmp);
    });
}
