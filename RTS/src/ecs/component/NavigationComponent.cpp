#include "stdafx.h"
#include "NavigationComponent.h"

#include "EntityComponentSystem.h"

const float MIN_DISTANCE = 2.0f;
const float ACCELERATION = 0.20f;

const std::string& NavigationComponentTable::NAME = "navigation";

inline void updateComponent(vecs::EntityID entity, NavigationComponent& cmp, EntityComponentSystem& ecs) {
	PhysicsComponent& myPhysCmp = ecs.getPhysicsComponentFromEntity(entity);
	const f32v2& offset = cmp.mTargetPos - myPhysCmp.mPosition;
	const float distance2 = glm::length2(offset);
	if (distance2 < MIN_DISTANCE) {
		cmp.mHasTarget = false;
		myPhysCmp.mFrictionEnabled = true;
	}
	else {
		// Disable friction while we are navigating
		myPhysCmp.mFrictionEnabled = false;
		const f32v2 targetVelocity = (offset / std::sqrt(distance2)) * cmp.speed;
		f32v2 velocityOffset = targetVelocity - myPhysCmp.mVelocity;
		float velocityDist = glm::length(velocityOffset);
		// TODO: DELTATIME
		if (velocityDist <= ACCELERATION) {
			myPhysCmp.mVelocity = targetVelocity;
		}
		else {
			myPhysCmp.mVelocity += (velocityOffset / velocityDist) * ACCELERATION;
		}
	}
}

void NavigationComponentTable::update(EntityComponentSystem& ecs) {
	// Update components
	for (auto&& cmp : *this) {
		if (cmp.second.mHasTarget) {
			updateComponent(cmp.first, cmp.second, ecs);
		}
	}
}
