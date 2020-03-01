#pragma once
#include "stdafx.h"

#include <box2d/b2_world_callbacks.h>

#include "actor/ActorTypes.h"
#include "ecs/component/PhysicsComponent.h"

class PhysQueryCallback : public b2QueryCallback
{
public:
	PhysQueryCallback(std::vector<EntityDistSortKey>& entities, f32v2 pos, const PhysicsComponentTable& physicsComponents, ActorTypesMask includeMask, ActorTypesMask excludeMask, float radius, vecs::EntityID except);
	virtual bool ReportFixture(b2Fixture* fixture) override;

	bool satisfiesMask(ActorTypes type) {
		return (type & mIncludeMask) && !(type & mExcludeMask);
	}

protected:
	std::vector<EntityDistSortKey>& mEntities;
	f32v2 mPos;
	const PhysicsComponentTable& mPhysicsComponents;
	ActorTypesMask mIncludeMask;
	ActorTypesMask mExcludeMask;
	float mRadius;
	vecs::EntityID mExcept;
};

class ArcQueryCallback : public PhysQueryCallback {
public:
	ArcQueryCallback(std::vector<EntityDistSortKey>& entities, f32v2 pos, const PhysicsComponentTable& physicsComponents, ActorTypesMask includeMask, ActorTypesMask excludeMask, float radius, vecs::EntityID except, f32v2 normal, float halfAngle, int quadrants);

	virtual bool ReportFixture(b2Fixture* fixture) override;

protected:
	f32v2 mNormal;
	float mHalfAngle;
	int mQuadrants;
};