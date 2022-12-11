#include "stdafx.h"
#include "PhysicsComponent.h"

#include <BulletDynamics/Dynamics/btRigidBody.h>
#include <BulletCollision/CollisionShapes/btCapsuleShape.h>

#include "world/IWorld.h"
#include "ecs/IEntityComponentSystem.h"

#include "resources/TileRepository.h"

#include "physics/PhysicsWorld.h"
constexpr float MIN_Z_SPEED = -0.24f;
constexpr float TOP_COLLISION_THRESHOLD = 0.75f;
constexpr float TOP_COLLISION_DEPTH = 1.0f - TOP_COLLISION_THRESHOLD;
constexpr float REFILTER_HEIGHT_CHANGE = 0.2f;
// This prevents tunelling when falling
static_assert(1.0f + MIN_Z_SPEED > TOP_COLLISION_THRESHOLD);

KEG_TYPE_DEF_SAME_NAME(PhysicsComponentDef, kt) {
    kt.addValue("shape", keg::Value::custom(offsetof(PhysicsComponentDef, colliderShape), "CollisionShapes", true));
    kt.addValue("mass", keg::Value::basic(offsetof(PhysicsComponentDef, massKg), keg::BasicType::F32));
    kt.addValue("half_dims", keg::Value::basic(offsetof(PhysicsComponentDef, halfExtents), keg::BasicType::F32_V3));
    kt.addValue("disable_xy_rot", keg::Value::basic(offsetof(PhysicsComponentDef, disableXyRot), keg::BasicType::BOOL));
    kt.addValue("disable_xyz_rot", keg::Value::basic(offsetof(PhysicsComponentDef, disableXyzRot), keg::BasicType::BOOL));
}

f32v2 PhysicsComponent::getDir() const {
    assert(IS_GAME_THREAD());
    btVector3 result = btVector3(1.0, 0.0, 0.0);
    result = mRigidBody->getWorldTransform() * result;
    return f32v2(result.getX(), result.getY());
}

f32v2 PhysicsComponent::getInterpolatedDir() const {
    assert(IS_GAME_THREAD());
    btVector3 result = btVector3(1.0, 0.0, 0.0);
    result = mRigidBody->getInterpolationWorldTransform() * result;
    return f32v2(result.getX(), result.getY());
}

#include "debugging/DebugRenderer.h"
f32v3 PhysicsComponent::getPosition() const {
    assert(IS_GAME_THREAD());
    // TODO: Physics system could cache position
    // TODO: Get origin?
    f32v3 rv = btVector3ToF32v3(mRigidBody->getWorldTransform().getOrigin());
    rv.z += mZPosOffset;
    return rv;
}

f32v3 PhysicsComponent::getInterpolatedPosition() const {
    assert(IS_GAME_THREAD());
    // TODO: Physics system could cache position
    f32v3 rv = btVector3ToF32v3(mRigidBody->getInterpolationWorldTransform().getOrigin());
    rv.z += mZPosOffset;
    return rv;
}

f32v3 PhysicsComponent::getLinearVelocity() const {
    assert(IS_GAME_THREAD());
    return btVector3ToF32v3(mRigidBody->getLinearVelocity());
}

f32 PhysicsComponent::getRotation() const {
    assert(IS_GAME_THREAD());
    // TODO: Interpolated or no?
    f32v2 dir = getDir();
    return atan2(dir.y, dir.x);
}

void PhysicsComponent::teleportToPoint(f32v3 worldPos) {
    assert(IS_GAME_THREAD());
    assert(mRigidBody);
    btTransform worldTransform;
    worldPos.z -= mZPosOffset;
    worldTransform.setOrigin(f32v3ToBtVector3(worldPos));
    worldTransform.setRotation(btQuaternion(0.0, 0.0, 0.0));
    mRigidBody->setWorldTransform(worldTransform);
}

void PhysicsComponent::setTransform(const f32v3& worldPos, f32 rotation) {
    assert(IS_GAME_THREAD());
    assert(mRigidBody);
    btTransform worldTransform;
    worldTransform.setOrigin(btVector3(worldPos.x, worldPos.y, worldPos.z - mZPosOffset));
    worldTransform.setRotation(btQuaternion(rotation, 0.0, 0.0));
    mRigidBody->setWorldTransform(worldTransform);
}

void PhysicsComponent::setVelocity(const f32v3& vel) {
    assert(IS_GAME_THREAD());
    assert(mRigidBody);
    mRigidBody->setLinearVelocity(f32v3ToBtVector3(vel));
}

