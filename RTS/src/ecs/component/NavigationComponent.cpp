#include "stdafx.h"
#include <glm/gtx/rotate_vector.hpp>
#include "NavigationComponent.h"

#include "EntityComponentSystem.h"

#include "World.h"

const float MIN_DISTANCE = 0.2f;
const float ACCELERATION = 0.013f;
constexpr int QUADRANTS = 5; //bad name

const std::string& NavigationComponentTable::NAME = "navigation";

inline void updateComponent(vecs::EntityID entity, NavigationComponent& cmp, EntityComponentSystem& ecs, World& world) {
	PhysicsComponent& myPhysCmp = ecs.getPhysicsComponentFromEntity(entity);
	const f32v2& offset = cmp.mTargetPos - myPhysCmp.getPosition();
	const float distance2 = glm::length2(offset);
	if (distance2 < MIN_DISTANCE) {
		cmp.mHasTarget = false;
		myPhysCmp.mFrictionEnabled = true;
	}
	else {


		f32v2 targetVelocity = (offset / std::sqrt(distance2)) * cmp.mSpeed/* * (cmp.mColliding ? 0.2f : 1.0f)*/;
		const f32v2 targetDir = glm::normalize(targetVelocity); // TODO: Get rid of normalize
		myPhysCmp.mDir = targetDir;

		const float ARC_LENGTH = DEG_TO_RAD(175.0f);

		// Look for undead allies
		std::vector<EntityDistSortKey> actors = world.queryActorsInArc(myPhysCmp.getPosition(), 5.0f, targetDir, ARC_LENGTH, ACTORTYPE_UNDEAD, true, QUADRANTS, entity);
		float closest[5] = { FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX };
		vecs::EntityID closestEnt[QUADRANTS];
		for (auto&& it : actors) {
			if (it.first.dist < closest[it.first.quadrant]) {
				closest[it.first.quadrant] = it.first.dist;
				closestEnt[it.first.quadrant] = it.second;
			}
		}
		 const int sequence[QUADRANTS] = { 2, 1, 3, 0, 4 }; // QUADRANTS 5
		// const int sequence[QUADRANTS] = { 1, 0, 3 }; // QUADRANTS 3
		int best = 1;
		float furthestDist = 0.0f;
		for (int i : sequence) {
			// Find the biggest gap prioritizing center
			if (closest[i] > furthestDist) {
				furthestDist = closest[i];
				best = i;
			}
		}
		// Flow into our selected gap
		// TODO: (remove branching?)
		if (best != 1) {
			const float SEGMENT_LENGTH = ARC_LENGTH / QUADRANTS;
			const float angles[QUADRANTS] = { -2 * SEGMENT_LENGTH, -SEGMENT_LENGTH, 0, SEGMENT_LENGTH, 2 * SEGMENT_LENGTH }; // QUADRANTS 5
			// const float angles[QUADRANTS] = { -SEGMENT_LENGTH, 0, SEGMENT_LENGTH }; // QUADRANTS 2
			targetVelocity = glm::rotate(targetVelocity, angles[best]);
		}

		// Disable friction while we are navigating
		//myPhysCmp.mFrictionEnabled = false;
		f32v2 velocityOffset = targetVelocity - myPhysCmp.getLinearVelocity();
		float velocityDist = glm::length(velocityOffset);
		// TODO: DELTATIME
		//myPhysCmp.mBody->ApplyForce(reinterpret_cast<const b2Vec2&>(targetVelocity * 0.025f), myPhysCmp.mBody->GetWorldCenter(), true);
		if (velocityDist <= ACCELERATION) {
			myPhysCmp.mBody->SetLinearVelocity(reinterpret_cast<const b2Vec2&>(targetVelocity));
		}
		else {
			const f32v2& currentLinearVelocity = reinterpret_cast<const f32v2&>(myPhysCmp.mBody->GetLinearVelocity());
			velocityOffset = (velocityOffset / velocityDist) * ACCELERATION + currentLinearVelocity;
			myPhysCmp.mBody->SetLinearVelocity(reinterpret_cast<const b2Vec2&>(velocityOffset));
		}
	}
}

void NavigationComponentTable::update(EntityComponentSystem& ecs, World& world) {
	// Update components
	for (auto&& cmp : *this) {
		if (isValid(cmp) && cmp.second.mHasTarget) {
			updateComponent(cmp.first, cmp.second, ecs, world);
		}
	}
}
