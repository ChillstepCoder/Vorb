#include "stdafx.h"
#include "PlayerControlComponent.h"

#include "EntityComponentSystem.h"

#include <Vorb/ui/InputDispatcher.h>

const std::string& PlayerControlComponentTable::NAME = "playercontrol";

const float SPEED = 0.3f;
const float ACCELERATION = 0.015f;

inline void updateComponent(vecs::EntityID entity, PlayerControlComponent& cmp, EntityComponentSystem& ecs) {
	UNUSED(cmp);
	PhysicsComponent& myPhysCmp = ecs.getPhysicsComponentFromEntity(entity);
	
	f32v2 moveDir(0.0f);

	if (vui::InputDispatcher::key.isKeyPressed(VKEY_W)) {
		moveDir.y = 1.0f;
	}
	else if (vui::InputDispatcher::key.isKeyPressed(VKEY_S)) {
		moveDir.y = -1.0f;
	}

	if (vui::InputDispatcher::key.isKeyPressed(VKEY_A)) {
		moveDir.x = -1.0f;
	}
	else if (vui::InputDispatcher::key.isKeyPressed(VKEY_D)) {
		moveDir.x = 1.0f;
	}

	moveDir = glm::normalize(moveDir);

	const f32v2 targetVelocity = moveDir * SPEED;
	f32v2 velocityOffset = targetVelocity - myPhysCmp.getLinearVelocity();
	float velocityDist = glm::length(velocityOffset);

	if (velocityDist <= ACCELERATION) {
		myPhysCmp.mBody->SetLinearVelocity(reinterpret_cast<const b2Vec2&>(targetVelocity));
	}
	else {
		const f32v2& currentLinearVelocity = reinterpret_cast<const f32v2&>(myPhysCmp.mBody->GetLinearVelocity());
		velocityOffset = (velocityOffset / velocityDist) * ACCELERATION + currentLinearVelocity;
		myPhysCmp.mBody->SetLinearVelocity(reinterpret_cast<const b2Vec2&>(velocityOffset));
	}
}

void PlayerControlComponentTable::update(EntityComponentSystem& ecs) {
	// Update components
	for (auto&& cmp : *this) {
		updateComponent(cmp.first, cmp.second, ecs);
	}
}
