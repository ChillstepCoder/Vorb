#include "stdafx.h"
#include "PlayerControlComponent.h"

#include "EntityComponentSystem.h"

#include "TileGrid.h"
#include "DebugRenderer.h"

#include <Vorb/ui/InputDispatcher.h>

const std::string& PlayerControlComponentTable::NAME = "playercontrol";

const float BASE_SPEED = 0.15f;
const float ACCELERATION = 0.015f;
const float IMPULSE = 0.02f;

const float ATTACK_RADIUS = 5.0f;
const float ATTACK_ARC_ANGLE = DEG_TO_RAD(120.0f);

void performAttack(vecs::EntityID entity, PlayerControlComponent& cmp, EntityComponentSystem& ecs, TileGrid& world) {
	PhysicsComponent& myPhysCmp = ecs.getPhysicsComponentFromEntity(entity);
	Combat::meleeAttackArc(entity, ecs.getCombatComponentFromEntity(entity), myPhysCmp.getPosition(), myPhysCmp.mDir, ATTACK_RADIUS, ATTACK_ARC_ANGLE, world, ecs);
}

f32v2 getMovementDir(TileGrid& world) {
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

	moveDir = world.convertScreenCoordToWorld(moveDir);
	return glm::normalize(moveDir);
}

void updateMovement(vecs::EntityID entity, PlayerControlComponent& cmp, EntityComponentSystem& ecs, TileGrid& world) {

	PhysicsComponent& myPhysCmp = ecs.getPhysicsComponentFromEntity(entity);

	bool isSprinting = cmp.mPlayerControlFlags & enum_cast(PlayerControlFlags::SPRINTING);
	const f32v2 moveDir = getMovementDir(world);

	if (moveDir.x == 0.0f && moveDir.y == 0.0f) {
		return;
	}
	// Facing
	if (isSprinting) {
		myPhysCmp.mDir = moveDir;
	}
	else {
		const f32v2& mousePos = world.getCurrentWorldMousePos();
		myPhysCmp.mDir = glm::normalize(mousePos - myPhysCmp.getPosition());
	}

	float speed = BASE_SPEED;
	float dotp = glm::dot(moveDir, glm::normalize(myPhysCmp.mDir));
	dotp = glm::clamp(dotp, -1.0f, 1.0f); // Fix any math rounding errors to prevent NAN acos
	const float angleOffset = acos(dotp);
	assert(angleOffset == angleOffset);
	// nan check
	// Reduce speed for backstep
	const float speedLerp = glm::clamp((angleOffset - M_PI_2) / M_PI_2, 0.0f, 1.0f);
	speed *= 1.0 - (speedLerp * 0.5f);

	const f32v2 targetVelocity = moveDir * speed * (isSprinting ? 1.0f : 0.5f);
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

inline void updateComponent(vecs::EntityID entity, PlayerControlComponent& cmp, EntityComponentSystem& ecs, TileGrid& world) {
	UNUSED(cmp);

	if (vui::InputDispatcher::key.isKeyPressed(VKEY_LSHIFT)) {
		cmp.mPlayerControlFlags |= enum_cast(PlayerControlFlags::SPRINTING);
	}
	else {
		cmp.mPlayerControlFlags &= ~enum_cast(PlayerControlFlags::SPRINTING);
	}

	updateMovement(entity, cmp, ecs, world);

	if (vui::InputDispatcher::key.isKeyPressed(VKEY_SPACE)) {
		performAttack(entity, cmp, ecs, world);
	}
}

void PlayerControlComponentTable::update(EntityComponentSystem& ecs, TileGrid& world) {
	// Update components
	for (auto&& cmp : *this) {
		updateComponent(cmp.first, cmp.second, ecs, world);
	}
}
