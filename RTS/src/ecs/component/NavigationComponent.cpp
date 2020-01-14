#include "stdafx.h"
#include "NavigationComponent.h"

#include "EntityComponentSystem.h"

const float MIN_DISTANCE = 0.2f;
const float ACCELERATION = 0.013f;

const std::string& NavigationComponentTable::NAME = "navigation";

inline void updateComponent(vecs::EntityID entity, NavigationComponent& cmp, EntityComponentSystem& ecs) {
	PhysicsComponent& myPhysCmp = ecs.getPhysicsComponentFromEntity(entity);
	const f32v2& offset = cmp.mTargetPos - myPhysCmp.getPosition();
	const float distance2 = glm::length2(offset);
	if (distance2 < MIN_DISTANCE) {
		cmp.mHasTarget = false;
		myPhysCmp.mFrictionEnabled = true;
	}
	else {
		// Disable friction while we are navigating
		//myPhysCmp.mFrictionEnabled = false;
		const f32v2 targetVelocity = (offset / std::sqrt(distance2)) * cmp.mSpeed * (cmp.mColliding ? 0.2f : 1.0f);
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

void NavigationComponentTable::update(EntityComponentSystem& ecs) {
	// Update components
	for (auto&& cmp : *this) {
		if (cmp.second.mHasTarget) {
			updateComponent(cmp.first, cmp.second, ecs);
		}
	}
}
