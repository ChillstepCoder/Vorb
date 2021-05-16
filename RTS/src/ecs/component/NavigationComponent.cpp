#include "stdafx.h"
#include <glm/gtx/rotate_vector.hpp>
#include "NavigationComponent.h"

#include "ecs/EntityComponentSystem.h"

#include "World.h"

const float MIN_DISTANCE = 0.2f;
const float ACCELERATION = 0.013f;
constexpr int QUADRANTS = 5; //bad name

inline void updateComponent(entt::entity entity, NavigationComponent& navCmp, PhysicsComponent& physCmp, World& world) {
	
	PathPoint& nextPoint = navCmp.mPath->points[navCmp.mCurrentPoint];

	const f32v2& offset = (f32v2(nextPoint) + f32v2(0.5f)) - physCmp.getXYPosition();
	const float distance2 = glm::length2(offset);
	if (distance2 < MIN_DISTANCE) {
        ++navCmp.mCurrentPoint;
		// Target reached TODO: Notify?
		if (navCmp.mCurrentPoint >= navCmp.mPath->numPoints) {
            physCmp.mFlags |= enum_cast(PhysicsComponentFlag::FRICTION_ENABLED);
		}
	}
	else {

		f32v2 targetVelocity = (offset / std::sqrt(distance2)) * navCmp.mSpeed/* * (cmp.mColliding ? 0.2f : 1.0f)*/;
		const f32v2 targetDir = glm::normalize(targetVelocity); // TODO: Get rid of normalize
		physCmp.mDir = targetDir;

		const float ARC_LENGTH = DEG_TO_RAD(175.0f);

		// Look for undead allies
		std::vector<EntityDistSortKey> actors = world.queryActorsInArc(physCmp.getXYPosition(), 5.0f, targetDir, ARC_LENGTH, ACTORTYPE_UNDEAD, ACTORTYPE_NONE, true, QUADRANTS, entity);
		float closest[5] = { FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX };
		entt::entity closestEnt[QUADRANTS];
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
		f32v2 velocityOffset = targetVelocity - physCmp.getLinearVelocity();
		float velocityDist = glm::length(velocityOffset);
		// TODO: DELTATIME
		//myPhysCmp.mBody->ApplyForce(reinterpret_cast<const b2Vec2&>(targetVelocity * 0.025f), myPhysCmp.mBody->GetWorldCenter(), true);
		if (velocityDist <= ACCELERATION) {
			physCmp.mBody->SetLinearVelocity(reinterpret_cast<const b2Vec2&>(targetVelocity));
		}
		else {
			const f32v2& currentLinearVelocity = reinterpret_cast<const f32v2&>(physCmp.mBody->GetLinearVelocity());
			velocityOffset = (velocityOffset / velocityDist) * ACCELERATION + currentLinearVelocity;
			physCmp.mBody->SetLinearVelocity(reinterpret_cast<const b2Vec2&>(velocityOffset));
		}
	}
}

void NavigationComponentSystem::update(entt::registry& registry, World& world, float deltaTime) {
	// Update components
    auto view = registry.view<NavigationComponent, PhysicsComponent>();

    for (auto entity : view) {
		auto& navCmp = view.get<NavigationComponent>(entity);
        if (navCmp.mCurrentPoint < navCmp.mPath->numPoints) {
            auto& physCmp = view.get<PhysicsComponent>(entity);
			updateComponent(entity, navCmp, physCmp, world);
		}
	}
}
