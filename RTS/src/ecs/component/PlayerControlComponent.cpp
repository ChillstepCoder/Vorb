#include "stdafx.h"
#include "PlayerControlComponent.h"

#include "ecs/EntityComponentSystem.h"

#include "World.h"
#include "DebugRenderer.h"

#include <Vorb/ui/InputDispatcher.h>

constexpr float ATTACK_RADIUS = 5.0f;
constexpr float ATTACK_ARC_ANGLE = DEG_TO_RAD(120.0f);

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

f32v2 getMovementDir() {
	f32v2 moveDir(0.0f);
	// TODO: Remove this
	int cartesianIndex = 0;//e_cast(clientData.worldLookCardinalDirection);
	const i32v2& axis = MOVEMENT_AXIS[cartesianIndex];

	// WSAD inputs
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


inline void updateComponent(entt::entity entity, PlayerControlComponent& controlCmp, LocomotionComponent& motionCmp, entt::registry& registry) {

	// Inputs for states
    if (vui::InputDispatcher::key.isKeyPressed(VKEY_SPACE)) {
		motionCmp.mMode = LocomotionMode::JUMP;
    }else if (vui::InputDispatcher::key.isKeyPressed(VKEY_LSHIFT)) {
		motionCmp.mMode = LocomotionMode::SPRINT;
	}
    else if(vui::InputDispatcher::key.isKeyPressed(VKEY_LCTRL)) {
        motionCmp.mMode = LocomotionMode::WALK;
    }
    else {
        motionCmp.mMode = LocomotionMode::RUN;
	}
	// Update skills
    if (vui::InputDispatcher::mouse.isButtonPressed(vorb::ui::MouseButton::LEFT)) {
		// TODO: Move this to some kind of combat manager/context
		CharacterModelComponent& modelCmp = registry.get<CharacterModelComponent>(entity);
		if (!modelCmp.mAnimState.mCurrentOneShotTrack.isActive()) {
			SkillsComponent& skillsCmp = registry.get<SkillsComponent>(entity);
			modelCmp.playOneShotAnimation(skillsCmp.mSkills[0]->mAnim);
		}
    }

	//  Update movement
    motionCmp.mDesiredDirection = getMovementDir();

    if (motionCmp.mDesiredDirection.x != 0.0f || motionCmp.mDesiredDirection.y != 0.0f) {
        // Remove any navigation component if we are applying movement input
        registry.remove<NavigationComponent>(entity);
	}
	else {
        motionCmp.mMode = LocomotionMode::IDLE;
	}

}

void PlayerControlSystem::update(entt::registry& registry, const Camera3D& camera) {
	// Update components
    auto view = registry.view<PlayerControlComponent, LocomotionComponent>();
    for (auto entity : view) {
		PlayerControlComponent& controlCmp = view.get<PlayerControlComponent>(entity);
		LocomotionComponent& motionCmp = view.get<LocomotionComponent>(entity);
		updateComponent(entity, controlCmp, motionCmp, registry);
	};
}
