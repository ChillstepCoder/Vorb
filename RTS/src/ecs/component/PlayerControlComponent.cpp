#include "stdafx.h"
#include "PlayerControlComponent.h"

#include "ecs/IEntityComponentSystem.h"

#include "world/World.h"
#include "debugging/DebugRenderer.h"

#include <Vorb/ui/InputDispatcher.h>
#include <glm/gtx/rotate_vector.hpp>

#include "camera/Camera3D.h"
#include "options/DebugOptions.h"

#include "rendering/RenderThreadTasks.h"

constexpr float ATTACK_RADIUS = 5.0f;
constexpr float ATTACK_ARC_ANGLE = DEG_TO_RAD(120.0f);

//void performAttack(vecs::EntityID entity, PlayerControlComponent& cmp, EntityComponentSystem& ecs, World& world) {
//	PhysicsComponent& myPhysCmp = ecs.getPhysicsComponentFromEntity(entity);
//	Combat::meleeAttackArc(entity, ecs.getCombatComponentFromEntity(entity), myPhysCmp.getPosition(), myPhysCmp.mDir, ATTACK_RADIUS, ATTACK_ARC_ANGLE, world, ecs);
//}

struct PlayerInputs {
    bool jump = false;
    bool sprint = false;
    bool walk = false;
    bool castFishingRod = false;
    bool primaryAction = false;
    bool forward = false;
    bool left = false;
    bool right = false;
    bool back = false;
};


f32v2 getMovementDir(const PlayerInputs& inputs, f32 cameraYaw) {
	f32v2 moveDir(0.0f);

	// WSAD inputs
    if (inputs.forward) {
        moveDir.x = 1.0f;
    }
    else if (inputs.back) {
        moveDir.x = -1.0f;
    }

    if (inputs.left) {
        moveDir.y = 1.0f;
    }
    else if (inputs.right) {
        moveDir.y = -1.0f;
    }

	// Normalize or return 0
	if (moveDir.x == 0.0f && moveDir.y == 0.0f) {
		return moveDir;
	}
	
	moveDir = glm::rotate(moveDir, cameraYaw);

	return glm::normalize(moveDir);
}


void PlayerControlSystem::updateComponent(World& world, entt::entity entity, PlayerControlComponent& playerControlCmp, CharacterControlComponent& characterControlCmp, entt::registry& registry, f32 cameraYaw) {

    PlayerInputs inputs;
    if (playerControlCmp.mInputLockCount == 0) {
        inputs.jump = vui::InputDispatcher::key.isKeyPressed(VKEY_SPACE);
        inputs.sprint = vui::InputDispatcher::key.isKeyPressed(VKEY_LSHIFT);
        inputs.walk = vui::InputDispatcher::key.isKeyPressed(VKEY_LCTRL);
        inputs.castFishingRod = vui::InputDispatcher::key.isKeyPressed(VKEY_G);
        inputs.primaryAction = vui::InputDispatcher::mouse.isButtonPressed(vorb::ui::MouseButton::LEFT);
        inputs.forward = vui::InputDispatcher::key.isKeyPressed(VKEY_W);
        inputs.left = vui::InputDispatcher::key.isKeyPressed(VKEY_A);
        inputs.right = vui::InputDispatcher::key.isKeyPressed(VKEY_D);
        inputs.back = vui::InputDispatcher::key.isKeyPressed(VKEY_S);
    }

    // Inputs for states, but only while we are on ground
    if (!characterControlCmp.isInAirState()) {
        if (inputs.jump) {
            characterControlCmp.mDesiredLocomotionMode = CharacterLocomotionMode::BEGIN_JUMP;
        }
        else if (inputs.sprint) {
            characterControlCmp.mDesiredLocomotionMode = CharacterLocomotionMode::SPRINT;
        }
        else if (inputs.walk) {
            characterControlCmp.mDesiredLocomotionMode = CharacterLocomotionMode::WALK;
        }
        else {
            characterControlCmp.mDesiredLocomotionMode = CharacterLocomotionMode::RUN;
        }

        // Fishing
        if (inputs.castFishingRod) {
            registry.get_or_emplace<FishingComponent>(entity).mIsCastInputPressed = true;
        }
        else {
            FishingComponent* component = registry.try_get<FishingComponent>(entity);
            if (component) {
                component->mIsCastInputPressed = false;
            }
        }
    }
	// Update skills
    if (inputs.primaryAction) {
        // TODO: Move this to some kind of combat manager/context
        SkillsComponent& skillsCmp = registry.get<SkillsComponent>(entity);
        world.getECS().mSkillsSystem.tryActivateSkillSlot(entity, registry, SkillSlot::Primary);
    }

	//  Update movement
    characterControlCmp.mMoveDirection = getMovementDir(inputs, cameraYaw);
    characterControlCmp.mFlags.clearBit(CharacterControlComponentFlags::ORIENT_TO_MOVEMENT);

    // Update controller rotation
    characterControlCmp.mControllerAngle = cameraYaw; // glm::rotate(f32v2(0.0f, 1.0f), -cameraYaw);

    if (characterControlCmp.mMoveDirection.x != 0.0f || characterControlCmp.mMoveDirection.y != 0.0f) {
        // Remove any navigation component if we are applying movement input
        registry.remove<NavigationComponent>(entity);
	}
	else if (!characterControlCmp.isInAirState() && characterControlCmp.mDesiredLocomotionMode != CharacterLocomotionMode::BEGIN_JUMP) {
        characterControlCmp.mDesiredLocomotionMode = CharacterLocomotionMode::IDLE;
	}

}

PlayerControlSystem::PlayerControlSystem() {

}

void PlayerControlSystem::update(World& world, entt::registry& registry, f32 cameraYaw) {
    ASSERT_GAME_THREAD();
    // Don't update while in free fly
    if (sDebugOptions.mCameraMode == CameraMode::FREE_LOOK) { return; }
	// Update components
    auto view = registry.view<PlayerControlComponent, CharacterControlComponent>();
    for (auto entity : view) {
		PlayerControlComponent& controlCmp = view.get<PlayerControlComponent>(entity);
		CharacterControlComponent& motionCmp = view.get<CharacterControlComponent>(entity);
		updateComponent(world, entity, controlCmp, motionCmp, registry, cameraYaw);
	};
}
