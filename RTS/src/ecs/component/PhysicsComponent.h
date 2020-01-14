#pragma once
#include "stdafx.h"

#include "actor/ActorTypes.h"
#include <Vorb/ecs/ComponentTable.hpp>

#include <box2d/b2_world.h>
#include <box2d/b2_body.h>

class b2Body;

enum class PhysicsComponentFlag : ui8 {
	AIRBORNE = 1 << 0
};

struct PhysicsComponent {

	const f32v2& getPosition() const {
		return reinterpret_cast<const f32v2&>(mBody->GetPosition());
	}

	const f32v2& getLinearVelocity() const {
		return reinterpret_cast<const f32v2&>(mBody->GetLinearVelocity());
	}

	ui8 mFlags = 0u;
	float mCollisionRadius = 0.0f;
	ActorTypes mQueryActorType = ACTORTYPE_NONE;
	bool mFrictionEnabled = true;

	b2Body* mBody = nullptr;
};

class PhysicsComponentTable : public vecs::ComponentTable<PhysicsComponent> {
public:
	PhysicsComponentTable(b2World& world);

	void update(float deltaTime);

	static const std::string& NAME;

	b2World& mWorld;
};
