#include "stdafx.h"
#include "PhysQueryCallback.h"

#include <box2d/b2_fixture.h>

PhysQueryCallback::PhysQueryCallback(std::vector<EntityDistSortKey>& entities, f32v2 pos, const PhysicsComponentTable& physicsComponents, ActorTypesMask includeMask, ActorTypesMask excludeMask, float radius, vecs::EntityID except)
	: mEntities(entities)
	, mPos(pos)
	, mPhysicsComponents(physicsComponents)
	, mIncludeMask(includeMask)
	, mExcludeMask(excludeMask)
	, mRadius(radius)
	, mExcept(except) {
}

bool PhysQueryCallback::ReportFixture(b2Fixture* fixture) {
	vecs::EntityID entityId = reinterpret_cast<vecs::EntityID>(fixture->GetUserData());
	if (entityId == mExcept) {
		return true;
	}
	// TODO: Marry physics component and fixture?
	const PhysicsComponent& cmp = mPhysicsComponents.getFromEntity(entityId);
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

ArcQueryCallback::ArcQueryCallback(std::vector<EntityDistSortKey>& entities, f32v2 pos, const PhysicsComponentTable& physicsComponents, ActorTypesMask includeMask, ActorTypesMask excludeMask, float radius, vecs::EntityID except, f32v2 normal, float halfAngle, int quadrants)
	: PhysQueryCallback(entities, pos, physicsComponents, includeMask, excludeMask, radius, except)
	, mNormal(normal)
	, mHalfAngle(halfAngle)
	, mQuadrants(quadrants) {
}

bool ArcQueryCallback::ReportFixture(b2Fixture* fixture) {
	vecs::EntityID entityId = reinterpret_cast<vecs::EntityID>(fixture->GetUserData());
	if (entityId == mExcept) {
		return true;
	}
	// TODO: Cache this math!
	const float quadrantSize = mHalfAngle * 2.0f / mQuadrants;
	const float quadrantStartAngle = (mQuadrants % 2 ? quadrantSize * 0.5f : 0.0f);

	const PhysicsComponent& cmp = mPhysicsComponents.getFromEntity(entityId);
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
				const int quadrant = floor((angle + mHalfAngle) / quadrantSize);
				mEntities.emplace_back(EntityDistInfo{ distanceToEdge, quadrant }, entityId);
			}
		}
	}

	// Return true to continue the query.
	return true;
}
