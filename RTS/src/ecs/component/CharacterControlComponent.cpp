#include "stdafx.h"
#include "CharacterControlComponent.h"

#include "ecs/component/PhysicsComponent.h"
#include "ecs/component/PositionComponent.h"

#include "options/GlobalMovementSettings.h"

#include "world/World.h"
#include "world/IHeightmapGrid.h"
#include "ecs/IFullECS.h"

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/ObjectLayer.h> // Used in CharacterBase
#include <Jolt/Physics/Character/CharacterBase.h>
#include <Jolt/Physics/Character/Character.h>

constexpr float JUMP_VELOCITY = 4.0f;

#ifdef DEBUG
static i32 sDbgTickCount = 0;
#endif

inline void updateComponent(World& world, entt::registry& registry, entt::entity entity, f32 elapsedSec) {
    PROFILE_FUNCTION();
    CharacterControlComponent& controlCmp = registry.get<CharacterControlComponent>(entity);
    PhysicsComponent& physCmp = registry.get<PhysicsComponent>(entity);
    PositionComponent& posCmp = registry.get<PositionComponent>(entity);

    //if (controlCmp.mFlags.isBitSet(CharacterControlComponentFlags::IsPlayerController)) {
    static constexpr float COLLISION_TOLERANCE = 0.05f;
    // TODO: Player too?
    JPH::Character* character = static_cast<JPH::Character*>(controlCmp.mCharacterController.get());
    {
       PROFILE_SCOPE("POST SIMULATION");
#ifdef DEBUG
       // Improve performance in debug mode as post simulation is expensive in debug
       if (sDbgTickCount == 0) {
           character->PostSimulation(COLLISION_TOLERANCE, false /*no lock*/);
       } 
#else
       character->PostSimulation(COLLISION_TOLERANCE, false /*no lock*/);
#endif
    }
    JPH::RVec3 rPos = character->GetPosition();
    posCmp.mPosition = f32v3(rPos.GetX(), rPos.GetY(), rPos.GetZ() - physCmp.mHalfHeight);
    const ChunkID newChunkID = world.getChunkIDAtWorldPos(posCmp.mPosition);

    if (newChunkID != posCmp.chunkId) [[unlikely]] {
        const ui32 oldChunkId = posCmp.chunkId;
        posCmp.chunkId = newChunkID;
        if (world.getECS().onEntityEnterNewChunk(entity, oldChunkId, newChunkID)) {
            return; // Entity was destroyed
        }
    }

    const IHeightmapGrid& grid = world.getHeightmapGrid();
    constexpr f32 SNAP_THRESHOLD = 0.1f;
    const f32 terrainHeight = grid.computeHeightAtPoint<false>(posCmp.mPosition);
    if (posCmp.mPosition.z + SNAP_THRESHOLD < terrainHeight) {
        posCmp.mPosition.z = terrainHeight;
        physCmp.teleportBottomToPoint(posCmp.mPosition);
        physCmp.clearLinearVelocityZIfNegative();
        physCmp.mFlags.setBit(PhysicsComponentFlag::IS_ON_GROUND);
    }

    // TODO: Simplify all these locomotion mode things
    bool onGround = true;
    switch (character->GetGroundState()) {
        case JPH::CharacterBase::EGroundState::OnGround: {
            if (controlCmp.mDesiredLocomotionMode == CharacterLocomotionMode::BEGIN_JUMP) {
                controlCmp.mDesiredLocomotionMode = CharacterLocomotionMode::JUMPING;
                controlCmp.mLocomotionMode = CharacterLocomotionMode::JUMPING;

                physCmp.addImpulse(f32v3(0.0f, 0.0f, JUMP_VELOCITY));
            }
            else if (controlCmp.mLocomotionMode == CharacterLocomotionMode::FALLING || controlCmp.mLocomotionMode == CharacterLocomotionMode::JUMPING) {
                controlCmp.mDesiredLocomotionMode = CharacterLocomotionMode::LANDING;
            }
            break;
        }
        case JPH::CharacterBase::EGroundState::OnSteepGround:
            // TODO: HANDLE
            break;
        case JPH::CharacterBase::EGroundState::NotSupported:
            break;
        case JPH::CharacterBase::EGroundState::InAir:
            break;
        default:
            break;
    }

    //f32 rotation = character->GetRotation().GetEulerAngles().GetZ();
    //LOG_INFO("Rotation: {}", rotation);

    if (controlCmp.mLocomotionMode != controlCmp.mDesiredLocomotionMode) {
        if (controlCmp.mLocomotionMode == CharacterLocomotionMode::LANDING) {
            // TODO: HMM IM NOT SURE ABOUT THISSSSS
           // constexpr f32 LANDING_ANIM_DURATION_MS = 200.0f;
           // if (controlCmp.mLandingTimer.stop() >= LANDING_ANIM_DURATION_MS) {
                controlCmp.mLocomotionMode = controlCmp.mDesiredLocomotionMode;
           // }
        }
        else {
            controlCmp.mLocomotionMode = controlCmp.mDesiredLocomotionMode;
        }
    }

    const f32v3 currentLinearVelocity = physCmp.getLinearVelocity();

    const bool isTryingToMove = (controlCmp.mMoveDirection.x != 0.0f || controlCmp.mMoveDirection.y != 0.0f);
    const f32 currentMaxSpeed = controlCmp.getCurrentMaxSpeed() * GlobalMovementSettings::speedMult;
    const f32 currentAcceleration = glm::min(controlCmp.getCurrentAcceleration() * GlobalMovementSettings::accelMult, 1.0f);

    f32v2 newLinearVelocity2D;
    if (isTryingToMove) {
        f32v2 desiredLinearVelocity2D;
        if (onGround || currentLinearVelocity.z > 0.0f) {
            // If we are on the ground or moving upwards, allow control
            desiredLinearVelocity2D = f32v2(controlCmp.mMoveDirection.x * currentMaxSpeed, controlCmp.mMoveDirection.y * currentMaxSpeed);
        }
        else {
            // Allow limited control while falling
            constexpr f32 FALL_CONTROL_MULT = 1.0f;
            desiredLinearVelocity2D = f32v2(controlCmp.mMoveDirection.x * FALL_CONTROL_MULT * currentMaxSpeed, controlCmp.mMoveDirection.y * FALL_CONTROL_MULT * currentMaxSpeed);
        }
        newLinearVelocity2D = MathUtil::lerpWithDeltaTime(f32v2(currentLinearVelocity), desiredLinearVelocity2D, currentAcceleration, elapsedSec);
    }
    else {
        // Decelerate
        constexpr f32 GROUND_FRICTION = 0.98f;
        constexpr f32 AIR_FRICTION = 0.8f;
        if (onGround) {
            newLinearVelocity2D = MathUtil::lerpWithDeltaTime(f32v2(currentLinearVelocity), f32v2(0.0f), GROUND_FRICTION, elapsedSec);
        }
        else {
            newLinearVelocity2D = MathUtil::lerpWithDeltaTime(f32v2(currentLinearVelocity), f32v2(0.0f), AIR_FRICTION, elapsedSec);
        }
    }

    physCmp.setLinearVelocity(f32v3(newLinearVelocity2D.x, newLinearVelocity2D.y, currentLinearVelocity.z));

    // Orient rotation to movement
    if (controlCmp.mFlags.isBitSet(CharacterControlComponentFlags::OrientToMovement)) {
        const f32 YAW_SPEED = 1.12f * GlobalMovementSettings::rotateSpeed;
        controlCmp.mControllerAngleRad = MathUtil::rotateYawToTarget(controlCmp.mControllerAngleRad, atan2(newLinearVelocity2D.y, newLinearVelocity2D.x), YAW_SPEED * elapsedSec);
    }

    // Use lerp instead of force so that we dont orbit
    // https://www.construct.net/en/blogs/ashleys-blog-2/using-lerp-delta-time-924
    //linearVelocity = linearVelocity.lerp(desiredVelocity, 1.0f - currentAcceleration);

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

void CharacterControlSystem::update(World& world, entt::registry& registry, f32 elapsedSec) {
    PROFILE_FUNCTION();

#ifdef DEBUG
    sDbgTickCount = (sDbgTickCount + 1) % 6;
#endif
    // Update components
    auto view = registry.view<CharacterControlComponent, PhysicsComponent, PositionComponent>();
    for (auto entity : view) {
        updateComponent(world, registry, entity, elapsedSec);
    }
}

CharacterControlComponent::CharacterControlComponent() = default;
CharacterControlComponent::~CharacterControlComponent() = default;
CharacterControlComponent::CharacterControlComponent(CharacterControlComponent&&) = default;
CharacterControlComponent& CharacterControlComponent::operator=(CharacterControlComponent&&) = default;