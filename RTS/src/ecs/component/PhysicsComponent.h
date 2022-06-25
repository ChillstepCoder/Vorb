#pragma once
#include "actor/ActorTypes.h"

#include "physics/CollisionShapes.h"

class World;
class EntityComponentSystem;
class PhysicsWorld;
class btRigidBody;

enum class PhysicsComponentFlag : ui8 {
	IS_ON_GROUND         = 1 << 0,
	LOCK_DIR_TO_VELOCITY = 1 << 1,
};

// A physics component represents the transform and optional rigid body 
class PhysicsComponent {
public:

	f32v2 getDir() const;
	f32v2 getInterpolatedDir() const;
	f32v3 getPosition() const;
	f32v3 getInterpolatedPosition() const;

	btRigidBody* mRigidBody = nullptr; // TODO: Pack btRigidBody?
    BitFlags<PhysicsComponentFlag> mFlags;

};
static_assert(sizeof(PhysicsComponent) == 16, "Keep tiny");

struct PhysicsComponentDef {
	CollisionShapes colliderShape = CollisionShapes::CAPSULE;
    f32v3 colliderScale = f32v3(1.0f);
	bool disableXyRot = false;
	bool disableXyzRot = false;
	float massKg = 0.0f;
};
KEG_TYPE_DECL(PhysicsComponentDef);

class PhysicsSystem {
public:
	void update(entt::registry& registry);

	World& mWorld;
};
