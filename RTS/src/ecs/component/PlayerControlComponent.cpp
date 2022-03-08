#include "stdafx.h"
#include "PlayerControlComponent.h"

#include "ecs/EntityComponentSystem.h"

#include "ecs/ClientEcsData.h"

#include "World.h"
#include "DebugRenderer.h"

#include <Vorb/ui/InputDispatcher.h>

constexpr float BASE_SPEED = 0.3f;
constexpr float ACCELERATION = 0.05f;

constexpr float ATTACK_RADIUS = 5.0f;
constexpr float ATTACK_ARC_ANGLE = DEG_TO_RAD(120.0f);

constexpr float JUMP_VELOCITY = 0.15f;

//void performAttack(vecs::EntityID entity, PlayerControlComponent& cmp, EntityComponentSystem& ecs, World& world) {
//	PhysicsComponent& myPhysCmp = ecs.getPhysicsComponentFromEntity(entity);
//	Combat::meleeAttackArc(entity, ecs.getCombatComponentFromEntity(entity), myPhysCmp.getPosition(), myPhysCmp.mDir, ATTACK_RADIUS, ATTACK_ARC_ANGLE, world, ecs);
//}

const i32v2 MOVEMENT_AXIS[4]{
	i32v2(AXIS_X, AXIS_Y), // Cartesian::DOWN
	i32v2(AXIS_Y, AXIS_X), // Cartesian::LEFT
	i32v2(AXIS_Y, AXIS_X), // Cartesian::RIGHT
	i32v2(AXIS_X, AXIS_Y), // Cartesian::UP
};
const f32v2 MOVEMENT_SIGNS[4]{
	f32v2(-1.0f, -1.0f), // Cartesian::DOWN
	f32v2(-1.0f, 1.0f),  // Cartesian::LEFT
	f32v2(1.0f, -1.0f),  // Cartesian::RIGHT
	f32v2(1.0f, 1.0f),   // Cartesian::UP
};

f32v2 getMovementDir(World& world, const ClientECSData& clientData) {
	f32v2 moveDir(0.0f);
	int cartesianIndex = e_cast(clientData.worldLookCardinalDirection);
	const i32v2& axis = MOVEMENT_AXIS[cartesianIndex];
	// Movement
	if (vui::InputDispatcher::key.isKeyPressed(VKEY_W)) {
		moveDir[axis.y] = MOVEMENT_SIGNS[cartesianIndex][0];
	}
	else if (vui::InputDispatcher::key.isKeyPressed(VKEY_S)) {
		moveDir[axis.y] = -MOVEMENT_SIGNS[cartesianIndex][0];
	}

	if (vui::InputDispatcher::key.isKeyPressed(VKEY_A)) {
		moveDir[axis.x] = -MOVEMENT_SIGNS[cartesianIndex][1];
	}
	else if (vui::InputDispatcher::key.isKeyPressed(VKEY_D)) {
		moveDir[axis.x] = MOVEMENT_SIGNS[cartesianIndex][1];
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

void updateMovement(PlayerControlComponent& controlCmp, PhysicsComponent& physCmp, World& world, const ClientECSData& clientData, entt::registry& registry) {

	bool isSprinting = controlCmp.mPlayerControlFlags & e_cast(PlayerControlFlags::SPRINTING);
	const f32v2 moveDir = getMovementDir(world, clientData);

	if (moveDir.x == 0.0f && moveDir.y == 0.0f) {
		return;
	}
	// Remove any navigation component if we are applying movement input
	entt::entity entityId = static_cast<entt::entity>(physCmp.mBody->GetUserData().pointer);
	registry.remove<NavigationComponent>(entityId);

	float speed = BASE_SPEED;
	float dotp = glm::dot(moveDir, glm::normalize(physCmp.mDir));
	dotp = glm::clamp(dotp, -1.0f, 1.0f); // Fix any math rounding errors to prevent NAN acos
	const float angleOffset = acos(dotp);
    assert(angleOffset == angleOffset); // nan check

	// Reduce speed for backstep
	const float speedLerp = glm::clamp((angleOffset - M_PI_2f) / M_PI_2f, 0.0f, 1.0f);
	speed *= 1.0f - (speedLerp * 0.5f);

	const f32v2 targetVelocity = moveDir * speed * (isSprinting ? 1.0f : 0.5f) * (vui::InputDispatcher::key.isKeyPressed(VKEY_LCTRL) ? 10000.0f : 1.0f);
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

inline void updateComponent(PlayerControlComponent& controlCmp, PhysicsComponent& physCmp, World& world, const ClientECSData& clientData, entt::registry& registry) {

	if (vui::InputDispatcher::key.isKeyPressed(VKEY_LSHIFT)) {
		controlCmp.mPlayerControlFlags |= e_cast(PlayerControlFlags::SPRINTING);
	}
	else {
		controlCmp.mPlayerControlFlags &= ~e_cast(PlayerControlFlags::SPRINTING);
	}

	updateMovement(controlCmp, physCmp, world, clientData, registry);

	// Jump
	if (vui::InputDispatcher::key.isKeyPressed(VKEY_SPACE)) {
		physCmp.setZVelocity(JUMP_VELOCITY);
	}
}

void PlayerControlSystem::update(entt::registry& registry, World& world, const ClientECSData& clientData) {
	// Update components
	registry.view<PlayerControlComponent, PhysicsComponent>().each([&](auto& controlCmp, auto& physCmp) {
		updateComponent(controlCmp, physCmp, world, clientData, registry);
	});
}
