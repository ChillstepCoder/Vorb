#include "stdafx.h"
#include "PhysQueryCallback.h"

#include <box2d/b2_fixture.h>

PhysQueryCallback::PhysQueryCallback(std::vector<EntityDistSortKey>& entities, f32v2 pos, const entt::registry& registry, ActorTypesMask includeMask, ActorTypesMask excludeMask, float radius, entt::entity except)
	: mEntities(entities)
	, mPos(pos)
	, mRegistry(registry)
	, mIncludeMask(includeMask)
	, mExcludeMask(excludeMask)
	, mRadius(radius)
	, mExcept(except) {
}

inline entt::entity extractEntity(b2Fixture* fixture) {
	return (entt::entity)reinterpret_cast<entt::id_type>(fixture->GetUserData());
}

bool PhysQueryCallback::ReportFixture(b2Fixture* fixture) {
	entt::entity entityId = extractEntity(fixture);
	if (entityId == mExcept) {
		return true;
	}
	// TODO: Marry physics component and fixture?
	const PhysicsComponent& cmp = mRegistry.get<PhysicsComponent>(entityId);
	if (satisfiesMask((ActorTypes)cmp.mQueryActorTypes)) {
		const f32v2 offset = cmp.getPosition() - mPos;
		const float distanceToEdge = glm::length(offset) - cmp.mCollisionRadius;
		if (distanceToEdge <= mRadius) {
			mEntities.emplace_back(EntityDistInfo{ distanceToEdge, 0 }, entityId);
		}
	}

	// Return true to continue the query.
	return true;
}

ArcQueryCallback::ArcQueryCallback(std::vector<EntityDistSortKey>& entities, f32v2 pos, const entt::registry& registry, ActorTypesMask includeMask, ActorTypesMask excludeMask, float radius, entt::entity except, f32v2 normal, float halfAngle, int quadrants)
	: PhysQueryCallback(entities, pos, registry, includeMask, excludeMask, radius, except)
	, mNormal(normal)
	, mHalfAngle(halfAngle)
	, mQuadrants(quadrants) {
}

bool ArcQueryCallback::ReportFixture(b2Fixture* fixture) {
	entt::entity entityId = extractEntity(fixture);
	if (entityId == mExcept) {
		return true;
	}
	// TODO: Cache this math!
	const float quadrantSize = mHalfAngle * 2.0f / mQuadrants;
	const float quadrantStartAngle = (mQuadrants % 2 ? quadrantSize * 0.5f : 0.0f);

    const PhysicsComponent& cmp = mRegistry.get<PhysicsComponent>(entityId);
	if (satisfiesMask((ActorTypes)cmp.mQueryActorTypes)) {
		f32v2 offset = cmp.getPosition() - mPos;
		const float offsetLength = glm::length(offset);
		const float distanceToEdge = offsetLength - cmp.mCollisionRadius;
		if (distanceToEdge <= mRadius) {
			offset /= offsetLength; // Normalize
			const float dot = glm::dot(mNormal, offset);
			const float det = mNormal.x * offset.y - mNormal.y * offset.x;
			float angle = atan2(det, dot);
			// TODO: Maybe don't do this logic when only 1 quadrant
			if (abs(angle) < mHalfAngle) {
				const int quadrant = (int)floor((angle + mHalfAngle) / quadrantSize);
				mEntities.emplace_back(EntityDistInfo{ distanceToEdge, quadrant }, entityId);
			}
		}
	}

	// Return true to continue the query.
	return true;
}
