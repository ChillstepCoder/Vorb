#pragma once
#include "actor/ActorTypes.h"

#include "physics/CollisionShapes.h"

class IEntityComponentSystem;
class PhysicsWorld;
class btRigidBody;
class World;

enum class PhysicsComponentFlag : ui8 {
	IS_ON_GROUND         = 1 << 0,
	LOCK_DIR_TO_VELOCITY = 1 << 1,
};

// A physics component represents the transform and optional rigid body 
// https://github.com/xissburg/edyn
class PhysicsComponent {
public:

	f32v2 getDir() const;
	f32v2 getInterpolatedDir() const;
    f32v3 getPosition() const;
	f32v3 getInterpolatedPosition() const;
	f32v3 getLinearVelocity() const;
	f32 getRotation() const;

	void teleportToPoint(f32v3 worldPos);

	void setTransform(const f32v3& pos, f32 rotation);
	void setVelocity(const f32v3& vel);

	// TODO: Delete rigidbody on component destroy
    btRigidBody* mRigidBody = nullptr; // TODO: Pack btRigidBody?
    f32 mZPosOffset = 0.0f; // Used for calculating the position at the bottom of the rigidbody
    BitFlags<PhysicsComponentFlag> mFlags;

};
static_assert(sizeof(PhysicsComponent) == 16, "Keep super tiny");

struct PhysicsComponentDef {
	CollisionShapes colliderShape = CollisionShapes::CAPSULE;
    f32v3 halfExtents = f32v3(1.0f);
	bool disableXyRot = false;
	bool disableXyzRot = false;
	float massKg = 0.0f; // By default is static
};
KEG_TYPE_DECL(PhysicsComponentDef);

class PhysicsSystem {
public:
    static void update(World& world, entt::registry& registry);
};
