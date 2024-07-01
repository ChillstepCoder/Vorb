#pragma once
#include "actor/ActorTypes.h"

#include "physics/CollisionShapes.h"
#include "ecs/component/ComponentDefBase.h"

class IFullECS;
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

    f32v3 getBottomPosition() const;
    f32v3 getLinearVelocity() const;
    f32 getLinearVelocityZ() const;

    void setLinearVelocity(f32v3 velocity);
    void setLinearVelocityZ(f32 zVelocity);
	// Set Z to 0 if it is going downwards only
	void clearLinearVelocityZIfNegative();
	void addImpulse(f32v3 impulse);

	// Teleport the middle of the object to the point
    void teleportToPoint(f32v3 worldPos);
	// Teleport the bottom of the object to the point (useful for characters)
	void teleportBottomToPoint(f32v3 worldPos);

    PhysBodyID mBodyID = INVALID_PHYS_BODY_ID;
    f32 mHalfHeight = 0.0f;
    BitFlags<PhysicsComponentFlag> mFlags;
	// TODO: Delete body on component destroy

};
//static_assert(sizeof(PhysicsComponent) == 16, "Keep super tiny");

class PhysicsComponentDef : public ComponentDefBase {
public:
	CollisionShapes colliderShape = CollisionShapes::CAPSULE;
    f32v3 halfExtents = f32v3(1.0f);
	bool disableXyRot = false;
	bool disableXyzRot = false;
	float massKg = 0.0f; // By default is static
};
SERIALIZABLE_SIMPLE(PhysicsComponentDef,
    make_field(o.colliderShape, "shape"sv),
	make_field(o.massKg, "mass"sv),
	make_field(o.halfExtents, "half_dims"sv),
	make_field(o.disableXyRot, "disable_xy_rot"sv),
	make_field(o.disableXyzRot, "disable_xyz_rot"sv)
);

class PhysicsSystem {
public:
    static void update(World& world, entt::registry& registry, f32 elapsedSec);
private:
	static void updateAngularVelocity(World& world, entt::registry& registry, f32 elapsedSec);
};
