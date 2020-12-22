#pragma once
#include "actor/ActorTypes.h"

#include <box2d/b2_body.h>

class World;
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
	PhysicsComponent(World& world, const f32v2& centerPosition, bool isStatic);
	void addCollider (entt::entity entityId, ColliderShapes shape, const float halfWidth);

    void setPosition(const f32v2& pos) {
        mBody->SetTransform(reinterpret_cast<const b2Vec2&>(pos), mBody->GetAngle());
    }
	void setLinearVelocity(const f32v2& vel) {
		mBody->SetLinearVelocity(reinterpret_cast<const b2Vec2&>(vel));
	}

	const f32v2& getPosition() const {
		return reinterpret_cast<const f32v2&>(mBody->GetPosition());
	}
	const f32v2& getLinearVelocity() const {
		return reinterpret_cast<const f32v2&>(mBody->GetLinearVelocity());
	}

    void teleportToPoint(const f32v2& worldPos) {
		mBody->SetTransform(reinterpret_cast<const b2Vec2&>(worldPos), mBody->GetAngle());
    }

	unsigned mQueryActorTypes = ACTORTYPE_NONE;
	f32v2 mDir = f32v2(0.0f, 1.0f);
	float mCollisionRadius = 0.0f;
	bool mFrictionEnabled = true; //TODO: Bit
	ui8 mFlags = 0u;

	b2Body* mBody = nullptr;
};

class PhysicsSystem {
public:
	PhysicsSystem(World& world);

	void update(entt::registry& registry, float deltaTime);

	World& mWorld;
};
