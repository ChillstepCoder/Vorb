#include "stdafx.h"
#include "PlayerControlComponent.h"

#include "EntityComponentSystem.h"

#include "ecs/ClientEcsData.h"

#include "World.h"
#include "DebugRenderer.h"

#include <Vorb/ui/InputDispatcher.h>

const float BASE_SPEED = 0.15f;
const float ACCELERATION = 0.015f;
const float IMPULSE = 0.02f;

const float ATTACK_RADIUS = 5.0f;
const float ATTACK_ARC_ANGLE = DEG_TO_RAD(120.0f);

//void performAttack(vecs::EntityID entity, PlayerControlComponent& cmp, EntityComponentSystem& ecs, World& world) {
//	PhysicsComponent& myPhysCmp = ecs.getPhysicsComponentFromEntity(entity);
//	Combat::meleeAttackArc(entity, ecs.getCombatComponentFromEntity(entity), myPhysCmp.getPosition(), myPhysCmp.mDir, ATTACK_RADIUS, ATTACK_ARC_ANGLE, world, ecs);
//}

f32v2 getMovementDir(World& world) {
	f32v2 moveDir(0.0f);
	// Movement
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

	// Normalize or return 0
	float length = glm::length(moveDir);
	if (length > FLT_EPSILON) {
		moveDir /= length;
	}
	else {
		return f32v2(0.0f);
	}

	return glm::normalize(moveDir);
}

void updateMovement(PlayerControlComponent& controlCmp, PhysicsComponent& physCmp, World& world, const ClientECSData& clientData) {

	bool isSprinting = controlCmp.mPlayerControlFlags & enum_cast(PlayerControlFlags::SPRINTING);
	const f32v2 moveDir = getMovementDir(world);

	if (moveDir.x == 0.0f && moveDir.y == 0.0f) {
		return;
	}
	// Facing
	if (isSprinting) {
		physCmp.mDir = moveDir;
	}
	else {
		physCmp.mDir = glm::normalize(clientData.worldMousePos - physCmp.getPosition());
	}

	float speed = BASE_SPEED;
	float dotp = glm::dot(moveDir, glm::normalize(physCmp.mDir));
	dotp = glm::clamp(dotp, -1.0f, 1.0f); // Fix any math rounding errors to prevent NAN acos
	const float angleOffset = acos(dotp);
	assert(angleOffset == angleOffset);
	// nan check
	// Reduce speed for backstep
	const float speedLerp = glm::clamp((angleOffset - M_PI_2f) / M_PI_2f, 0.0f, 1.0f);
	speed *= 1.0f - (speedLerp * 0.5f);

	const f32v2 targetVelocity = moveDir * speed * (isSprinting ? 2.0f : 0.5f) * (vui::InputDispatcher::key.isKeyPressed(VKEY_LCTRL) ? 10000.0f : 1.0f);
	f32v2 velocityOffset = targetVelocity - physCmp.getLinearVelocity();
	float velocityDist = glm::length(velocityOffset);

	const float acceleration = ACCELERATION * (vui::InputDispatcher::key.isKeyPressed(VKEY_LCTRL) ? 5.0f : 1.0f);

	if (velocityDist <= acceleration) {
		physCmp.mBody->SetLinearVelocity(reinterpret_cast<const b2Vec2&>(targetVelocity));
	}
	else {
		const f32v2& currentLinearVelocity = reinterpret_cast<const f32v2&>(physCmp.mBody->GetLinearVelocity());
		velocityOffset = (velocityOffset / velocityDist) * acceleration + currentLinearVelocity;
		physCmp.mBody->SetLinearVelocity(reinterpret_cast<const b2Vec2&>(velocityOffset));
	}
}

inline void updateComponent(PlayerControlComponent& controlCmp, PhysicsComponent& physCmp, World& world, const ClientECSData& clientData) {

	if (vui::InputDispatcher::key.isKeyPressed(VKEY_LSHIFT)) {
		controlCmp.mPlayerControlFlags |= enum_cast(PlayerControlFlags::SPRINTING);
	}
	else {
		controlCmp.mPlayerControlFlags &= ~enum_cast(PlayerControlFlags::SPRINTING);
	}

	updateMovement(controlCmp, physCmp, world, clientData);

	if (vui::InputDispatcher::key.isKeyPressed(VKEY_SPACE)) {
	//	performAttack(entity, cmp, ecs, world);
	}
}

void PlayerControlSystem::update(entt::registry& registry, World& world, const ClientECSData& clientData) {
	// Update components
	registry.view<PlayerControlComponent, PhysicsComponent>().each([&](auto& controlCmp, auto& physCmp) {
		updateComponent(controlCmp, physCmp, world, clientData);
	});
}
