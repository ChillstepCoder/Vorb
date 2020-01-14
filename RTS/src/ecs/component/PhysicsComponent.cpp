#include "stdafx.h"
#include "PhysicsComponent.h"

#include <box2d/b2_body.h>

const std::string& PhysicsComponentTable::NAME = "physics";

// TODO: Capsule collision
inline void handleCollision2D(PhysicsComponent& cmp1, PhysicsComponent& cmp2) {
	//// We add radius since position is the top left corner
	//const glm::vec2 distVec = cmp2.getPosition() - cmp1.getPosition();
	//const float dist = glm::length(distVec);
	//const float totalRadius = cmp1.mCollisionRadius + cmp2.mCollisionRadius;
	//const float collisionDepth = totalRadius - dist;
	//// Check for collision
	//if (collisionDepth > 0) {
	//	const glm::vec2 distDir = distVec / dist;

	//	// Push away the balls based on ratio of mMasses
	//	// TODO: Could/should we encode this in the impulse?
	//	// TODO: 3D?
	//	const f32v2 offset = distDir * collisionDepth * 0.5f;
	//	cmp1.getPosition() -= offset * (cmp2.mMass / cmp1.mMass);
	//	cmp2.mPosition += offset * (cmp1.mMass / cmp2.mMass);

	//	// Calculate deflection. http://stackoverflow.com/a/345863
	//	// Fixed thanks to youtube user Sketchy502
	//	const float aci = glm::dot(cmp1.mVelocity, distDir);
	//	const float bci = glm::dot(cmp2.mVelocity, distDir);

	//	const float acf = (aci * (cmp1.mMass - cmp2.mMass) + 2 * cmp2.mMass * bci) / (cmp1.mMass + cmp2.mMass);
	//	const float bcf = (bci * (cmp2.mMass - cmp1.mMass) + 2 * cmp1.mMass * aci) / (cmp1.mMass + cmp2.mMass);

	//	cmp1.mVelocity += (acf - aci) * distDir;
	//	cmp2.mVelocity += (bcf - bci) * distDir;
	//}
}

// TODO: Measure perf of this vs non inline vs macro
inline void updateComponent(PhysicsComponent& cmp, float deltaTime) {
	//if (cmp.mPosition.z <= 0.0f) {
	//}
	//cmp.mPosition += cmp.mVelocity;
	//if (cmp.mFrictionEnabled) {
	//	cmp.mVelocity -= cmp.mVelocity * (1.0f - cmp.mFrictionCoef) * deltaTime; // TODO: Deterministic?
	//}
}


PhysicsComponentTable::PhysicsComponentTable(b2World& world) 
	: mWorld(world) {

}

void PhysicsComponentTable::update(float deltaTime) {
	if (getComponentListSize() <= 1) {
		return;
	}

	// Collision
	// TODO: Spatial Partition
	// Skip default element
	std::vector<ComponentPairing>::iterator it = _components.begin() + 1;
	while (it != _components.end()) {
		auto compareIt = it;
		while (++compareIt != _components.end()) {
			handleCollision2D(it->second, compareIt->second);
		}
		++it;
	}

	// Update components
	for (auto&& cmp : _components) {
		updateComponent(cmp.second, deltaTime);
	}
}
