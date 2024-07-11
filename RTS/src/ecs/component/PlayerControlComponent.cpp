#include "stdafx.h"
#include "PlayerControlComponent.h"

#include "ecs/IFullECS.h"

#include "world/World.h"
#include "debugging/DebugRenderer.h"

#include <Vorb/ui/InputDispatcher.h>
#include <glm/gtx/rotate_vector.hpp>

#include "physics/PhysicsWorld.h"

#include "options/DebugOptions.h"

#include "rendering/RenderThreadTasks.h"
#include "camera/Camera3DGameThreadData.h"

#include "debugging/DebugRenderer.h"

// TODO: REMOVE
#include "ecs/factory/EntityFactory.h"

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
    bool interact = false;
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


void PlayerControlSystem::updateComponent(entt::entity entity, PlayerControlComponent& playerControlCmp, CharacterControlComponent& characterControlCmp, const Camera3DGameThreadData& cameraData, f32 elapsedSec) {
    PROFILE_FUNCTION();

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
        inputs.interact = vui::InputDispatcher::key.isKeyPressed(VKEY_E);
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
            mRegistry.get_or_emplace<FishingComponent>(entity).mIsCastInputPressed = true;
        }
        else {
            FishingComponent* component = mRegistry.try_get<FishingComponent>(entity);
            if (component) {
                component->mIsCastInputPressed = false;
            }
        }
    }
	// Update skills
    if (inputs.primaryAction) {
        // TODO: Move this to some kind of combat manager/context
        SkillsComponent& skillsCmp = mRegistry.get<SkillsComponent>(entity);
        mWorld.getECS().mSkillsSystem.tryActivateSkillSlot(entity, mRegistry, SkillSlot::Primary);
    }

	//  Update movement
    characterControlCmp.mMoveDirection = getMovementDir(inputs, cameraData.yaw);
    characterControlCmp.mFlags.clearBit(CharacterControlComponentFlags::OrientToMovement);

    // Update controller rotation
    characterControlCmp.mControllerAngleRad = cameraData.yaw;

    if (characterControlCmp.mMoveDirection.x != 0.0f || characterControlCmp.mMoveDirection.y != 0.0f) {
        // Remove any navigation component if we are applying movement input
        mRegistry.remove<NavigationComponent>(entity);
	}
	else if (!characterControlCmp.isInAirState() && characterControlCmp.mDesiredLocomotionMode != CharacterLocomotionMode::BEGIN_JUMP) {
        characterControlCmp.mDesiredLocomotionMode = CharacterLocomotionMode::IDLE;
	}
    
    
    updateSelection(entity, playerControlCmp, cameraData, inputs, elapsedSec);

}

void PlayerControlSystem::updateSelection(entt::entity entity, PlayerControlComponent& playerControlCmp, const Camera3DGameThreadData& cameraData, const PlayerInputs& inputs, f32 elapsedSec) {
    // Interact input
    bool didInteract = false;
    if (inputs.interact) {
        if (playerControlCmp.mInteractDuration == 0.0f) {
            didInteract = true;
        }
        playerControlCmp.mInteractDuration += elapsedSec;
    }
    else {
        playerControlCmp.mInteractDuration = 0.0f;
    }

    // Reset so we can select it anew below
    playerControlCmp.mSelectedObjectData.modelId = INVALID_MODEL_ID;

    // Selection
    const f32 rayLength = 9.0f;
    // TODO: Filters?
    PhysHitResult result = mWorld.getPhysicsWorld().raycastFirst(cameraData.worldPos, cameraData.worldPos + cameraData.direction * rayLength);
    if (result.didHit()) {
        // TODO: Tiles as well?
        PhysicsBodyUserDataType type = result.mBodyUserData.getType();
        if (type == PhysicsBodyUserDataType::Entity || type == PhysicsBodyUserDataType::ItemEntity) {
            entt::entity selected = result.mBodyUserData.getEntity();

            // Interact
            if (inputs.interact) {
                EntityFactory::destroyEntity(mWorld, selected);\
                return;
            }

            // Selection
            if (DynamicModelComponent* modelCmp = mRegistry.try_get<DynamicModelComponent>(selected)) {
                playerControlCmp.mSelectedObjectData.modelId = modelCmp->modelId;
                playerControlCmp.mSelectedObjectData.scale = modelCmp->scale;
            } else if (StaticModelComponent* modelCmp = mRegistry.try_get<StaticModelComponent>(selected)) {
                playerControlCmp.mSelectedObjectData.modelId = modelCmp->modelId;
                playerControlCmp.mSelectedObjectData.scale = modelCmp->scale;
            } else {
                playerControlCmp.mSelectedObjectData.modelId = INVALID_MODEL_ID;
                // We can only select models
                return;
            }

            if (SimpleTextNameplateComponent* nameplateCmp = mRegistry.try_get<SimpleTextNameplateComponent>(selected)) {
                playerControlCmp.mSelectedObjectData.text = nameplateCmp->text;
                playerControlCmp.mSelectedObjectData.textColor = nameplateCmp->color;
                playerControlCmp.mSelectedObjectData.textZOffset = nameplateCmp->zOffset;
            } else {
                playerControlCmp.mSelectedObjectData.text.clear();
            }

            playerControlCmp.mSelectedObjectData.position = mRegistry.get<PositionComponent>(selected).mPosition;
            if (OrientationComponent* orientationCmp = mRegistry.try_get<OrientationComponent>(selected)) {
                playerControlCmp.mSelectedObjectData.orientation = orientationCmp->mOrientation;
            }
            else {
                playerControlCmp.mSelectedObjectData.orientation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
            }
        }
    }
    else {
        playerControlCmp.mSelectedObjectData.modelId = INVALID_MODEL_ID;
    }

}

PlayerControlSystem::PlayerControlSystem(World& world, entt::registry& registry) : mWorld(world), mRegistry(registry) {

}

void PlayerControlSystem::update(const Camera3DGameThreadData& cameraData, f32 elapsedSec) {
    ASSERT_GAME_THREAD();
    // Don't update while in free fly
    if (sDebugOptions.mCameraMode == CameraMode::FREE_LOOK) { return; }
	// Update components
    auto view = mRegistry.view<PlayerControlComponent, CharacterControlComponent>();
    for (auto entity : view) {
		PlayerControlComponent& controlCmp = view.get<PlayerControlComponent>(entity);
		CharacterControlComponent& motionCmp = view.get<CharacterControlComponent>(entity);
		updateComponent(entity, controlCmp, motionCmp, cameraData, elapsedSec);
	};
}
