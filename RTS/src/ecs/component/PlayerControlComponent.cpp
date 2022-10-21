#include "stdafx.h"
#include "PlayerControlComponent.h"

#include "ecs/IEntityComponentSystem.h"

#include "world/IWorld.h"
#include "debugging/DebugRenderer.h"

#include <Vorb/ui/InputDispatcher.h>
#include <glm/gtx/rotate_vector.hpp>

#include "camera/Camera3D.h"
#include "options/DebugOptions.h"

constexpr float ATTACK_RADIUS = 5.0f;
constexpr float ATTACK_ARC_ANGLE = DEG_TO_RAD(120.0f);

//void performAttack(vecs::EntityID entity, PlayerControlComponent& cmp, EntityComponentSystem& ecs, World& world) {
//	PhysicsComponent& myPhysCmp = ecs.getPhysicsComponentFromEntity(entity);
//	Combat::meleeAttackArc(entity, ecs.getCombatComponentFromEntity(entity), myPhysCmp.getPosition(), myPhysCmp.mDir, ATTACK_RADIUS, ATTACK_ARC_ANGLE, world, ecs);
//}


f32v2 getMovementDir(f32 cameraYaw) {
	f32v2 moveDir(0.0f);

	// WSAD inputs
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
	if (moveDir.x == 0.0f && moveDir.y == 0.0f) {
		return moveDir;
	}
	
	moveDir = glm::rotate(moveDir, -cameraYaw);

	return glm::normalize(moveDir);
}


void PlayerControlSystem::updateComponent(entt::entity entity, PlayerControlComponent& controlCmp, CharacterControlComponent& motionCmp, entt::registry& registry, f32 cameraYaw) {

    // Inputs for states, but only while we are on ground
    if (!motionCmp.isInAirState()) {
        if (vui::InputDispatcher::key.isKeyPressed(VKEY_SPACE)) {
            motionCmp.mDesiredMode = LocomotionMode::BEGIN_JUMP;
        }
        else if (vui::InputDispatcher::key.isKeyPressed(VKEY_LSHIFT)) {
            motionCmp.mDesiredMode = LocomotionMode::SPRINT;
        }
        else if (vui::InputDispatcher::key.isKeyPressed(VKEY_LCTRL)) {
            motionCmp.mDesiredMode = LocomotionMode::WALK;
        }
        else {
            motionCmp.mDesiredMode = LocomotionMode::RUN;
        }
    }
	// Update skills
    if (vui::InputDispatcher::mouse.isButtonPressed(vorb::ui::MouseButton::LEFT)) {
		// TODO: Move this to some kind of combat manager/context
        CharacterModelComponent& modelCmp = registry.get<CharacterModelComponent>(entity);
        if (!modelCmp.mAnimState->mCurrentOneShotTrack.isActive()) {
            SkillsComponent& skillsCmp = registry.get<SkillsComponent>(entity);
            modelCmp.playOneShotAnimation(skillsCmp.mSkills[0]->mAnim);
        }
    }

	//  Update movement
    motionCmp.mMoveDirection = getMovementDir(cameraYaw);

    // Update controller rotation
    motionCmp.mControllerDirection = glm::rotate(f32v2(0.0f, 1.0f), -cameraYaw);

    if (motionCmp.mMoveDirection.x != 0.0f || motionCmp.mMoveDirection.y != 0.0f) {
        // Remove any navigation component if we are applying movement input
        registry.remove<NavigationComponent>(entity);
	}
	else if (!motionCmp.isInAirState() && motionCmp.mDesiredMode != LocomotionMode::BEGIN_JUMP) {
        motionCmp.mDesiredMode = LocomotionMode::IDLE;
	}

}

PlayerControlSystem::PlayerControlSystem() {

}

void PlayerControlSystem::update(entt::registry& registry, f32 cameraYaw) {
    assert(IS_GAME_THREAD());
    // Don't update while in free fly
    if (sDebugOptions.mCameraMode == CameraMode::FREE_LOOK) { return; }
	// Update components
    auto view = registry.view<PlayerControlComponent, CharacterControlComponent>();
    for (auto entity : view) {
		PlayerControlComponent& controlCmp = view.get<PlayerControlComponent>(entity);
		CharacterControlComponent& motionCmp = view.get<CharacterControlComponent>(entity);
		updateComponent(entity, controlCmp, motionCmp, registry, cameraYaw);
	};
}
