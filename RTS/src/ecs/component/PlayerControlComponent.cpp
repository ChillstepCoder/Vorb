#include "stdafx.h"
#include "PlayerControlComponent.h"

#include "ecs/EntityComponentSystem.h"

#include "World.h"
#include "DebugRenderer.h"

#include <Vorb/ui/InputDispatcher.h>
#include <glm/gtx/rotate_vector.hpp>

#include "camera/Camera3D.h"

constexpr float ATTACK_RADIUS = 5.0f;
constexpr float ATTACK_ARC_ANGLE = DEG_TO_RAD(120.0f);

//void performAttack(vecs::EntityID entity, PlayerControlComponent& cmp, EntityComponentSystem& ecs, World& world) {
//	PhysicsComponent& myPhysCmp = ecs.getPhysicsComponentFromEntity(entity);
//	Combat::meleeAttackArc(entity, ecs.getCombatComponentFromEntity(entity), myPhysCmp.getPosition(), myPhysCmp.mDir, ATTACK_RADIUS, ATTACK_ARC_ANGLE, world, ecs);
//}


f32v2 getMovementDir(const Camera3D& camera) {
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
	
	moveDir = glm::rotate(moveDir, -camera.getYaw());

	return glm::normalize(moveDir);
}


inline void updateComponent(entt::entity entity, PlayerControlComponent& controlCmp, LocomotionComponent& motionCmp, entt::registry& registry, const Camera3D& camera) {

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
		if (!modelCmp.mAnimState.mCurrentOneShotTrack.isActive()) {
			SkillsComponent& skillsCmp = registry.get<SkillsComponent>(entity);
			modelCmp.playOneShotAnimation(skillsCmp.mSkills[0]->mAnim);
		}
    }

	//  Update movement
    motionCmp.mDesiredDirection = getMovementDir(camera);

    if (motionCmp.mDesiredDirection.x != 0.0f || motionCmp.mDesiredDirection.y != 0.0f) {
        // Remove any navigation component if we are applying movement input
        registry.remove<NavigationComponent>(entity);
	}
	else if (!motionCmp.isInAirState() && motionCmp.mDesiredMode != LocomotionMode::BEGIN_JUMP) {
        motionCmp.mDesiredMode = LocomotionMode::IDLE;
	}

}

void PlayerControlSystem::update(entt::registry& registry, const Camera3D& camera) {
	// Update components
    auto view = registry.view<PlayerControlComponent, LocomotionComponent>();
    for (auto entity : view) {
		PlayerControlComponent& controlCmp = view.get<PlayerControlComponent>(entity);
		LocomotionComponent& motionCmp = view.get<LocomotionComponent>(entity);
		updateComponent(entity, controlCmp, motionCmp, registry, camera);
	};
}
