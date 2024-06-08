#include "stdafx.h"
#include "PhysicsComponent.h"

#include <BulletDynamics/Dynamics/btRigidBody.h>
#include <BulletCollision/CollisionShapes/btCapsuleShape.h>

#include "world/World.h"
#include "world/IHeightmapGrid.h"
#include "ecs/IFullECS.h"

#include "resources/TileRepository.h"

#include "physics/PhysicsWorld.h"
constexpr float MIN_Z_SPEED = -0.24f;
constexpr float TOP_COLLISION_THRESHOLD = 0.75f;
constexpr float TOP_COLLISION_DEPTH = 1.0f - TOP_COLLISION_THRESHOLD;
constexpr float REFILTER_HEIGHT_CHANGE = 0.2f;
// This prevents tunelling when falling
static_assert(1.0f + MIN_Z_SPEED > TOP_COLLISION_THRESHOLD);



f32v2 PhysicsComponent::getDir() const {
    ASSERT_GAME_THREAD();
    btVector3 result = btVector3(1.0, 0.0, 0.0);
    result = mRigidBody->getWorldTransform().getBasis() * result;
    return f32v2(result.getX(), result.getY());
}

f32v2 PhysicsComponent::getInterpolatedDir() const {
    ASSERT_GAME_THREAD();
    btVector3 result = btVector3(1.0, 0.0, 0.0);
    result = mRigidBody->getInterpolationWorldTransform().getBasis() * result;
    return f32v2(result.getX(), result.getY());
}

f32v3 PhysicsComponent::getPosition() const {
    ASSERT_GAME_THREAD();
    // TODO: Physics system could cache position
    // TODO: Get origin?
    f32v3 rv = btVector3ToF32v3(mRigidBody->getWorldTransform().getOrigin());
    rv.z += mZPosOffset;
    return rv;
}

f32v3 PhysicsComponent::getInterpolatedPosition() const {
    ASSERT_GAME_THREAD();
    // TODO: Physics system could cache position
    f32v3 rv = btVector3ToF32v3(mRigidBody->getInterpolationWorldTransform().getOrigin());
    rv.z += mZPosOffset;
    return rv;
}

f32v3 PhysicsComponent::getLinearVelocity() const {
    ASSERT_GAME_THREAD();
    return btVector3ToF32v3(mRigidBody->getLinearVelocity());
}

f32v2 PhysicsComponent::getLinearVelocity2D() const {
    ASSERT_GAME_THREAD();
    return btVector3ToF32v2(mRigidBody->getLinearVelocity());
}

f32 PhysicsComponent::getRotation() const {
    ASSERT_GAME_THREAD();
    // TODO: Interpolated or no?
    f32v2 dir = getDir();
    return atan2(dir.y, dir.x);
}

void PhysicsComponent::teleportToPoint(f32v3 worldPos) {
    ASSERT_GAME_THREAD();
    assert(mRigidBody);
    btTransform worldTransform;
    worldPos.z -= mZPosOffset;
    worldTransform.setOrigin(f32v3ToBtVector3(worldPos));
    worldTransform.setRotation(btQuaternion(0.0, 0.0, 0.0));
    mRigidBody->setWorldTransform(worldTransform);
}

void PhysicsComponent::setTransform(const f32v3& worldPos, f32 rotation) {
    ASSERT_GAME_THREAD();
    assert(mRigidBody);
    btTransform worldTransform;
    worldTransform.setOrigin(btVector3(worldPos.x, worldPos.y, worldPos.z - mZPosOffset));
    worldTransform.setRotation(btQuaternion(rotation, 0.0, 0.0));
    mRigidBody->setWorldTransform(worldTransform);
}

void PhysicsComponent::setVelocity(const f32v3& vel) {
    ASSERT_GAME_THREAD();
    assert(mRigidBody);
    mRigidBody->setLinearVelocity(f32v3ToBtVector3(vel));
}

void PhysicsSystem::update(World& world, entt::registry& registry) {
    PROFILE_FUNCTION();
    const IHeightmapGrid& grid = world.getHeightmapGrid();
    auto view = registry.view<PhysicsComponent, PositionComponent>();
    for (auto entity : view) {
        PhysicsComponent& cmp = view.get<PhysicsComponent>(entity);
        f32v3 pos = cmp.getPosition();
        const f32v2 xyPosition(pos.x, pos.y);
        constexpr f32 SNAP_THRESHOLD = 0.01f;
        const f32 terrainHeight = grid.computeHeightAtPoint<false>(xyPosition);

        if (terrainHeight >= pos.z - SNAP_THRESHOLD) {
            f32v3 vel = cmp.getLinearVelocity();
            const f32v3 velocity = cmp.getLinearVelocity();
            if (velocity.z < 0.0f) {
                cmp.setVelocity(f32v3(vel.x, vel.y, 0.0f));
            }
            pos.z = terrainHeight;
            cmp.setTransform(pos, 0.0f);
            cmp.mFlags.setBit(PhysicsComponentFlag::IS_ON_GROUND);
        }
        else {
            cmp.mFlags.clearBit(PhysicsComponentFlag::IS_ON_GROUND);
        }
        // Copy position to our position component
        PositionComponent& posCmp = view.get<PositionComponent>(entity);
        posCmp.mPosition = pos;
        const ChunkID newChunkID = world.getChunkIDAtWorldPos(pos);

        if (newChunkID != posCmp.chunkId) [[unlikely]] {
            const ui32 oldChunkId = posCmp.chunkId;
            posCmp.chunkId = newChunkID;
            world.getECS().onEntityEnterNewChunk(entity, oldChunkId, newChunkID);
            // Entity may be destroyed in onEntityEnterNewChunk
        }
    };
}
