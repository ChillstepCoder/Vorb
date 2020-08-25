#pragma once
#include "actor/ActorTypes.h"

#include <Vorb/ecs/ECS.h>
#include <Vorb/ecs/ComponentTable.hpp>

#include <box2d/b2_world.h>
#include <box2d/b2_body.h>

class b2Body;
class EntityComponentSystem;

enum class PhysicsComponentFlag : ui8 {
	AIRBORNE = 1 << 0,
	LOCK_DIR_TO_VELOCITY
};

enum class ColliderShapes {
	SPHERE,
	NONE
};

class PhysicsComponent {
public:
	void initBody(EntityComponentSystem& parentSystem, const f32v2& centerPosition, bool isStatic);
	void addCollider (vecs::EntityID entityId, ColliderShapes shape, const float halfWidth);

	const f32v2& getPosition() const {
		return reinterpret_cast<const f32v2&>(mBody->GetPosition());
	}

	const f32v2& getLinearVelocity() const {
		return reinterpret_cast<const f32v2&>(mBody->GetLinearVelocity());
	}

	unsigned mQueryActorTypes = ACTORTYPE_NONE;
	f32v2 mDir = f32v2(0.0f, 1.0f);
	float mCollisionRadius = 0.0f; // TODO: needed?
	bool mFrictionEnabled = true; //TODO: Bit
	ui8 mFlags = 0u;

	b2Body* mBody = nullptr;
};

class PhysicsComponentTable : public vecs::ComponentTable<PhysicsComponent> {
public:
	PhysicsComponentTable(b2World& world);

	void update(float deltaTime);

	static const std::string& NAME;

	b2World& mWorld;
};
