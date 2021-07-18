#pragma once
#include <box2d/b2_world_callbacks.h>

#include "actor/ActorTypes.h"
#include "ecs/component/PhysicsComponent.h"

class PhysQueryCallback : public b2QueryCallback
{
public:
	PhysQueryCallback(std::vector<EntityDistSortKey>& entities, f32v2 pos, const entt::registry& registry, ActorTypesMask includeMask, ActorTypesMask excludeMask, float radius, entt::entity except);
	virtual bool ReportFixture(b2Fixture* fixture) override;

	bool satisfiesMask(ActorTypes type) {
		return (type & mIncludeMask) && !(type & mExcludeMask);
	}

protected:
	std::vector<EntityDistSortKey>& mEntities;
	f32v2 mPos;
	const entt::registry& mRegistry;
	ActorTypesMask mIncludeMask;
	ActorTypesMask mExcludeMask;
	float mRadius;
	entt::entity mExcept;
};

class ArcQueryCallback : public PhysQueryCallback {
public:
	ArcQueryCallback(std::vector<EntityDistSortKey>& entities, f32v2 pos, const entt::registry& registry, ActorTypesMask includeMask, ActorTypesMask excludeMask, float radius, entt::entity except, f32v2 normal, float halfAngle, int quadrants);

	virtual bool ReportFixture(b2Fixture* fixture) override;

protected:
	f32v2 mNormal;
	float mHalfAngle;
	int mQuadrants;
};