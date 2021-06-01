#pragma once
#include "actor/ActorTypes.h"

#include <box2d/b2_body.h>

class World;
class b2Body;
class EntityComponentSystem;

enum class PhysicsComponentFlag : ui8 {
	AIRBORNE             = 1 << 0,
	LOCK_DIR_TO_VELOCITY = 1 << 1,
	FRICTION_ENABLED     = 1 << 2,
};

enum class ColliderShapes {
	NONE,
	CIRCLE,
	COUNT
};
KEG_ENUM_DECL(ColliderShapes);

class PhysicsComponent {
public:
	PhysicsComponent(World& world, const f32v2& centerPosition, bool isStatic);

	void addCollider (entt::entity entityId, ColliderShapes shape, const float halfWidth);

    void setXYPosition(const f32v2& pos) {
        mBody->SetTransform(reinterpret_cast<const b2Vec2&>(pos), mBody->GetAngle());
    }
	void setLinearVelocity(const f32v2& vel) {
		mBody->SetLinearVelocity(reinterpret_cast<const b2Vec2&>(vel));
	}
	void setZPosition(f32 z) {
		mZPosition = z;
	}
	void setZVelocity(f32 vel) {
		mZVelocity = vel;
	}

	const f32v2& getXYPosition() const {
		return reinterpret_cast<const f32v2&>(mBody->GetPosition());
	}
	const float getZPosition() const {
		return mZPosition;
	}

	const f32v2& getLinearVelocity() const {
		return reinterpret_cast<const f32v2&>(mBody->GetLinearVelocity());
	}

    void teleportToPoint(const f32v2& worldPos) {
		mBody->SetTransform(reinterpret_cast<const b2Vec2&>(worldPos), mBody->GetAngle());
		if (mZPosition < 0.0f) {
			mZPosition = 2.0f;
		}
    }

	f32v2 mDir = f32v2(0.0f, 1.0f);
    f32 mCollisionRadius = 0.0f;
    f32 mZPosition = 0.0f;
    f32 mZVelocity = 0.0f;
    b2Body* mBody = nullptr;

    ui8 mFlags = 0u;
    ui8 mQueryActorTypes = ACTORTYPE_NONE;

};

struct PhysicsComponentDef {
    ColliderShapes colliderShape;
    float colliderRadius;
    bool isStatic;
};
KEG_TYPE_DECL(PhysicsComponentDef);

class PhysicsSystem {
public:
	PhysicsSystem(World& world);

	void update(entt::registry& registry);

	World& mWorld;
};
