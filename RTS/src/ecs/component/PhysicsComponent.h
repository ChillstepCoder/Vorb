#pragma once
#include "stdafx.h"

#include "actor/ActorTypes.h"
#include <Vorb/ecs/ComponentTable.hpp>

enum class PhysicsComponentFlag : ui8 {
	AIRBORNE = 1 << 0
};

struct PhysicsComponent {
	f32v2 mPosition = f32v2(0.0f);
	f32v2 mVelocity = f32v2(0.0f);
	float mMass = 0.0f;
	float mGravity = 0.0f;
	// Zero radius means no collide
	float mCollisionRadius = 0.0f;
	// Multiplied per update
	float mFrictionCoef = 0.0f;
	ui8 mFlags = 0u;
	ActorTypes mQueryActorType = ACTORTYPE_NONE;
	bool mFrictionEnabled = true;
};

class PhysicsComponentTable : public vecs::ComponentTable<PhysicsComponent> {
public:
	static const std::string& NAME;

	void update(float deltaTime);
};
