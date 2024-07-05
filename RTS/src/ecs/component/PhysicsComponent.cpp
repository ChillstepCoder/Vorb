#include "stdafx.h"
#include "PhysicsComponent.h"


#include "world/World.h"
#include "world/IHeightmapGrid.h"
#include "ecs/IFullECS.h"

#include "resources/TileRepository.h"

#include "physics/PhysicsWorld.h"

#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/BodyLockInterface.h>

constexpr float MIN_Z_SPEED = -0.24f;
constexpr float TOP_COLLISION_THRESHOLD = 0.75f;
constexpr float TOP_COLLISION_DEPTH = 1.0f - TOP_COLLISION_THRESHOLD;
constexpr float REFILTER_HEIGHT_CHANGE = 0.2f;
// This prevents tunelling when falling
static_assert(1.0f + MIN_Z_SPEED > TOP_COLLISION_THRESHOLD);

// TODO: USE A COLLISION TRANSFORM ON THE ITEM
static const JPH::Quat ROTATE_ZUP = JPH::Quat::sRotation(JPH::Vec3::sAxisZ(), JPH::JPH_PI * 0.5f);

f32v3 PhysicsComponent::getBottomPosition() const {
    const JPH::BodyInterface& bodyInterface = PhysicsWorldBodyInterface::getBodyInterfaceNonLocking(*sGamePhysicsWorld);

    JPH::RVec3 rPos = bodyInterface.GetPosition(JPH::BodyID(mBodyID));
    return f32v3(rPos.GetX(), rPos.GetY(), rPos.GetZ() - mHalfHeight);
}

f32v3 PhysicsComponent::getLinearVelocity() const {
    const JPH::BodyInterface& bodyInterface = PhysicsWorldBodyInterface::getBodyInterfaceNonLocking(*sGamePhysicsWorld);

    JPH::Vec3 vel = bodyInterface.GetLinearVelocity(JPH::BodyID(mBodyID));
    return f32v3(vel.GetX(), vel.GetY(), vel.GetZ());
}

f32 PhysicsComponent::getLinearVelocityZ() const {
    const JPH::BodyInterface& bodyInterface = PhysicsWorldBodyInterface::getBodyInterfaceNonLocking(*sGamePhysicsWorld);
    return bodyInterface.GetLinearVelocity(JPH::BodyID(mBodyID)).GetZ();
}

glm::quat PhysicsComponent::getOrientation() const {
    const JPH::BodyInterface& bodyInterface = PhysicsWorldBodyInterface::getBodyInterfaceNonLocking(*sGamePhysicsWorld);
    const JPH::Quat q = bodyInterface.GetRotation(JPH::BodyID(mBodyID)) * ROTATE_ZUP;
    glm::quat gameOrientation = glm::quat(
        q.GetW(),
        q.GetX(),
        q.GetY(), // Note: Y and Z swapped
        q.GetZ() // and Y negated
    );
    return gameOrientation;
}

void PhysicsComponent::setLinearVelocity(f32v3 velocity) {
    ASSERT_GAME_THREAD();
    assert(mBodyID != INVALID_PHYS_BODY_ID);

    JPH::BodyInterface& bodyInterface = PhysicsWorldBodyInterface::getBodyInterface(*sGamePhysicsWorld);
    bodyInterface.SetLinearVelocity(JPH::BodyID(mBodyID), JPH::Vec3(velocity.x, velocity.y, velocity.z));
}

void PhysicsComponent::setLinearVelocityZ(f32 zVelocity) {
    ASSERT_GAME_THREAD();
    assert(mBodyID != INVALID_PHYS_BODY_ID);

    f32v3 linearVelocity = getLinearVelocity();

    JPH::BodyInterface& bodyInterface = PhysicsWorldBodyInterface::getBodyInterface(*sGamePhysicsWorld);
    bodyInterface.SetLinearVelocity(JPH::BodyID(mBodyID), JPH::Vec3(linearVelocity.x, linearVelocity.y, zVelocity));
}

void PhysicsComponent::clearLinearVelocityZIfNegative() {
    ASSERT_GAME_THREAD();
    assert(mBodyID != INVALID_PHYS_BODY_ID);

    f32v3 linearVelocity = getLinearVelocity();
    if (linearVelocity.z < 0.0f) {
        linearVelocity.z = 0.0f;
        JPH::BodyInterface& bodyInterface = PhysicsWorldBodyInterface::getBodyInterface(*sGamePhysicsWorld);
        bodyInterface.SetLinearVelocity(JPH::BodyID(mBodyID), JPH::Vec3(linearVelocity.x, linearVelocity.y, 0.0f));
    }
}

void PhysicsComponent::addImpulse(f32v3 impulse) {
    ASSERT_GAME_THREAD();
    assert(mBodyID != INVALID_PHYS_BODY_ID);

    JPH::BodyInterface& bodyInterface = PhysicsWorldBodyInterface::getBodyInterface(*sGamePhysicsWorld);
    bodyInterface.AddImpulse(JPH::BodyID(mBodyID), JPH::Vec3(impulse.x, impulse.y, impulse.z));
}

void PhysicsComponent::teleportToPoint(f32v3 worldPos) {
    ASSERT_GAME_THREAD();
    assert(mBodyID != INVALID_PHYS_BODY_ID);

    JPH::BodyInterface& bodyInterface = PhysicsWorldBodyInterface::getBodyInterface(*sGamePhysicsWorld);
    bodyInterface.SetPosition(JPH::BodyID(mBodyID), JPH::DVec3((double)worldPos.x, (double)worldPos.y, (double)worldPos.z), JPH::EActivation::Activate);
}

void PhysicsComponent::teleportBottomToPoint(f32v3 worldPos) {
    teleportToPoint(f32v3(worldPos.x, worldPos.y, worldPos.z + mHalfHeight));
}

void PhysicsSystem::update(World& world, entt::registry& registry, f32 elapsedSec) {
    ASSERT_GAME_THREAD();
    PROFILE_FUNCTION();
    const IHeightmapGrid& grid = world.getHeightmapGrid();

    // Update all uncontrolled object positions
    // Exclude character control because it has its own update.
    // Exclude TileItemComponent because they are deactivated physics objects
    auto view = registry.view<PhysicsComponent, PositionComponent>(entt::exclude<CharacterControlComponent, TileItemComponent>);
    for (auto entity : view) {
        PhysicsComponent& cmp = view.get<PhysicsComponent>(entity);
        PositionComponent& posCmp = view.get<PositionComponent>(entity);
        f32v3 pos = cmp.getBottomPosition();
        const f32v2 xyPosition(pos.x, pos.y);
        constexpr f32 SNAP_THRESHOLD = 0.1f;
        const f32 terrainHeight = grid.computeHeightAtPoint<false>(xyPosition);

        if (pos.z + SNAP_THRESHOLD < terrainHeight) {
            pos.z = terrainHeight;
            cmp.teleportBottomToPoint(pos);
            cmp.clearLinearVelocityZIfNegative();
            cmp.mFlags.setBit(PhysicsComponentFlag::IS_ON_GROUND);
        }
        else {
            cmp.mFlags.clearBit(PhysicsComponentFlag::IS_ON_GROUND);
        }

        // Copy position to our position component
        posCmp.mPosition = pos;
        const ChunkID newChunkID = world.getChunkIDAtWorldPos(pos);

        if (newChunkID != posCmp.chunkId) [[unlikely]] {
            const ui32 oldChunkId = posCmp.chunkId;
            posCmp.chunkId = newChunkID;
            if (world.getECS().onEntityEnterNewChunk(entity, oldChunkId, newChunkID)) {
                continue; // Entity deleted
            }
        }

        // Optional orientation tracking
        if (OrientationComponent* oCmp = registry.try_get<OrientationComponent>(entity)) {
            oCmp->mOrientation = cmp.getOrientation();
        }
    };

    updateAngularVelocity(world, registry, elapsedSec);
}

void PhysicsSystem::updateAngularVelocity(World& world, entt::registry& registry, f32 elapsedSec) {
    auto view = registry.view<OrientationComponent, AngularVelocityComponent>();
    for (auto entity : view) {
        OrientationComponent& ocmp = view.get<OrientationComponent>(entity);
        AngularVelocityComponent& acmp = view.get<AngularVelocityComponent>(entity);
        ocmp.mOrientation = acmp.applyToRotation(elapsedSec, ocmp.mOrientation);
    }
}