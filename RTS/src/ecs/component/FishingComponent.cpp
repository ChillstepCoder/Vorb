#include "stdafx.h"
#include "FishingComponent.h"

#include "ecs/component/PhysicsComponent.h"
#include "ecs/component/CharacterControlComponent.h"
#include "ecs/component/PlayerControlComponent.h"

#include "world/ecosystem/FishEcosystem.h"
#include "debugging/DebugRenderer.h"

#include "gamethread/GameThreadTasks.h"

#include "world/World.h"
#include "world/IHeightmapGrid.h"
#include "time/TimestepManager.h"

#include "ui/UIContext.h"
#include "ui/minigame/FishingMinigame.h"
#include "ui/minigame/LocalMinigameContext.h"

#include "physics/PhysicsConst.h"

#include "resources/ResourceManager.h"
#include "resources/FishRepository.h"

#include <Vorb/ui/InputDispatcher.h>

// Initialize launch position
constexpr f32 BOBBER_GRAVITY = GRAVITY_Z;

FishingComponentSystem::FishingComponentSystem() {
    vui::InputDispatcher::mouse.addButtonDownListener([this](const vui::MouseButtonEvent& buttonEvent) {
        // TODO: See header, an input queue which we fill is probably better than managing atomic state
        if (buttonEvent.button == vui::MouseButton::LEFT) {
            mWasButtonPressed = true;
        }
    });

    mLocalPlayerMinigameGameThreadData = std::make_unique<FishingMinigameGameThreadData>();
}

FishingComponentSystem::~FishingComponentSystem() {

}

void FishingComponentSystem::update(World& world, entt::registry& registry, f32 elapsedSec) {

    mTimeStep = Services::TimestepManager::ref().getTimestepSec();

    auto view = registry.view<FishingComponent, PhysicsComponent, CharacterControlComponent>();

    std::vector<entt::entity> componentsToRemove;

    for (auto entity : view) {
        auto& fishCmp = view.get<FishingComponent>(entity);
        auto& physCmp = view.get<PhysicsComponent>(entity);
        auto& controlCmp = view.get<CharacterControlComponent>(entity);
        updateFishing(world, registry, entity, fishCmp, physCmp, controlCmp, elapsedSec);
        if (fishCmp.isDone()) {
            if (fishCmp.mIsLocalPlayer) {
                if (mLocalPlayerControlLocked) {
                    --registry.get<PlayerControlComponent>(entity).mInputLockCount;
                    mLocalPlayerControlLocked = false;
                }
            }
            // TODO: Notify inventory of caught fish and such? minigame result?
            // Tell fish we are done
            if (fishCmp.mTargetFish != INVALID_ENTITY) {
                FishEcosystem& fishEcosystem = world.getFishEcosystem();
                if (fishCmp.mState == FishingComponentState::Success) {
                    fishEcosystem.setFishCaught(fishCmp.mTargetFish, entity);
                }
                else {
                    fishEcosystem.clearFishFollowTarget(fishCmp.mTargetFish);
                }
            }
            componentsToRemove.emplace_back(entity);
        }
    }

    for (auto&& entity : componentsToRemove) {
        registry.remove<FishingComponent>(entity);
    }
}

f32v3 getCastTarget(FishingComponent& fishCmp, PhysicsComponent& physCmp, CharacterControlComponent& controlCmp) {
    const f32v2 controllerDir = controlCmp.getControllerDir();
    f32v3 castTarget = physCmp.getPosition();
    castTarget.z = 0.0f;
    castTarget.x += controllerDir.x * fishCmp.mCastCharge;
    castTarget.y += controllerDir.y * fishCmp.mCastCharge;
    return castTarget;
}

void castLine(FishingComponent& fishCmp, PhysicsComponent& physCmp, CharacterControlComponent& controlCmp) {
    constexpr f32 totalCastTimeSec = 1.0f;
    f32v3 castTargetPosition = getCastTarget(fishCmp, physCmp, controlCmp);

    const f32v2 playerDir = controlCmp.getControllerDir();


    // Starting the launch position a bit up in the air and a bit forward from the player (Z is up)
    fishCmp.mBobberPosition = physCmp.getPosition() + f32v3(playerDir.x, playerDir.y, 1.1f);

    // Compute initial velocities using equation of motion
    fishCmp.mBobberVelocity = MathUtil::computeInitialProjectileVelocityToTarget(fishCmp.mBobberPosition, castTargetPosition, totalCastTimeSec, BOBBER_GRAVITY);
    fishCmp.mState = FishingComponentState::Casted;
}

void FishingComponentSystem::updateFishing(World& world, entt::registry& registry, entt::entity entity, FishingComponent& fishingCmp, PhysicsComponent& physCmp, CharacterControlComponent& controlCmp, f32 elapsedSec) {
    ASSERT_GAME_THREAD();
    // TODO: Configurable
    constexpr f32 RETICLE_DIMS = 0.5f;
    constexpr f32 CASTING_POWER = 0.1f;
    constexpr f32 MIN_CAST_DISTANCE = 1.0f;
    constexpr f32 MAX_CAST_DISTANCE = 10.0f;

    // TODO: SEPARATE FROM PLAYER SO NPC CAN DO IT
    bool wasMousePressed = mWasButtonPressed;
    mWasButtonPressed = false;
    switch (fishingCmp.mState) {
        case FishingComponentState::Initializing:
            fishingCmp.mState = FishingComponentState::Casting;
            [[fallthrough]];
        case FishingComponentState::Casting: {
            if (fishingCmp.mIsCastInputPressed) {
                fishingCmp.mCastCharge += CASTING_POWER;
                fishingCmp.mCastCharge = glm::min(fishingCmp.mCastCharge, MAX_CAST_DISTANCE);
            }
            else if (fishingCmp.mCastCharge > MIN_CAST_DISTANCE) {
                castLine(fishingCmp, physCmp, controlCmp);
                return;
            } else {
                fishingCmp.mState = FishingComponentState::Fail;
                return;
            }

            fishingCmp.mTargetPosition = getCastTarget(fishingCmp, physCmp, controlCmp);
            DebugRenderer::drawWireQuadThreadSafe(fishingCmp.mTargetPosition - f32v3(RETICLE_DIMS * 0.5f, RETICLE_DIMS * 0.5f, 0.0f), f32v2(RETICLE_DIMS), color4(1.0f - fishingCmp.mCastCharge / MAX_CAST_DISTANCE, fishingCmp.mCastCharge / MAX_CAST_DISTANCE, 0.0f), 2);
            break;
        }
        case FishingComponentState::Casted: {
            fishingCmp.mBobberVelocity.z += BOBBER_GRAVITY * mTimeStep;
            fishingCmp.mBobberPosition += fishingCmp.mBobberVelocity * mTimeStep;
            DebugRenderer::drawWireQuadThreadSafe(fishingCmp.mBobberPosition - f32v3(RETICLE_DIMS * 0.5f, RETICLE_DIMS * 0.5f, 0.0f), f32v2(RETICLE_DIMS), color4(1.0f - fishingCmp.mCastCharge / MAX_CAST_DISTANCE, fishingCmp.mCastCharge / MAX_CAST_DISTANCE, 0.0f), 2);
            // Water impact
            // TODO: True water plane position
            if (fishingCmp.mBobberPosition.z <= 0.0f) {
                fishingCmp.mBobberPosition.z = 0.0f;
                fishingCmp.mBobberVelocity = f32v3(0.0f);
                fishingCmp.mState = FishingComponentState::Fishing;
            }
            else if (fishingCmp.mBobberPosition.z <= world.getHeightmapGrid().computeHeightAtPoint<false>(f32v2(fishingCmp.mBobberPosition))) {
                // Terrain collision is failure
                fishingCmp.mState = FishingComponentState::Fail;
                return;
            }
            break;
        }
        case FishingComponentState::Fishing: {
            constexpr f32 PULL_ACCELLERATION = 10.0f;
            constexpr f32 MAX_PULL_SPEED = 5.0f;
            constexpr f32 BOBBER_DRAG = 0.95f;
            constexpr f32 FISH_ATTRACT_DISTANCE = 10.0f;
            // Bobber physics
            fishingCmp.mBobberPosition += fishingCmp.mBobberVelocity * mTimeStep;
            fishingCmp.mBobberVelocity *= BOBBER_DRAG;
            // Reeling
            if (fishingCmp.mIsCastInputPressed) {
                f32v2 distanceVec = physCmp.getPosition() - fishingCmp.mBobberPosition;
                f32v2 pullNormal = glm::normalize(distanceVec);
                fishingCmp.mBobberVelocity += f32v3(pullNormal.x, pullNormal.y, 0.0f) * PULL_ACCELLERATION * mTimeStep;
                const f32 bobberSpeed = glm::length(fishingCmp.mBobberVelocity);
                if (bobberSpeed > MAX_PULL_SPEED) {
                    fishingCmp.mBobberVelocity == (fishingCmp.mBobberVelocity / bobberSpeed) * MAX_PULL_SPEED;
                }
                // Check for getting too close to the player
                if (glm::length2(distanceVec) <= SQ(MIN_CAST_DISTANCE)) {
                    fishingCmp.mState = FishingComponentState::Fail;
                    return;
                }
                // Check for beaching the bobber
                const f32 terrainHeight = world.getHeightmapGrid().computeHeightAtPoint<false>(f32v2(fishingCmp.mBobberPosition));
                // TODO: If tile handle is invalid, switch to LOD?
                if (terrainHeight >= -0.01f) {
                    fishingCmp.mState = FishingComponentState::Fail;
                    return;
                }
            }

            // TODO Server version
            if (fishingCmp.mTargetFish == INVALID_ENTITY) {
                FishEcosystem& fishEcosystem = world.getFishEcosystem();
                PreciseTimer timer;
                entt::entity closestFish = fishEcosystem.getClosestIdleFishToPoint(fishingCmp.mBobberPosition, FISH_ATTRACT_DISTANCE);
                if (closestFish != INVALID_ENTITY) {
                    fishingCmp.mTargetFish = closestFish;
                    fishEcosystem.setFishFollowBobber(closestFish, entity);
                }
                LOG_WARN("CLOSEST FOUND IN {} ms", timer.stop());
                return;
            }

            DebugRenderer::drawWireQuadThreadSafe(fishingCmp.mBobberPosition - f32v3(RETICLE_DIMS * 0.5f, RETICLE_DIMS * 0.5f, 0.0f), f32v2(RETICLE_DIMS), color4(1.0f - fishingCmp.mCastCharge / MAX_CAST_DISTANCE, fishingCmp.mCastCharge / MAX_CAST_DISTANCE, 0.0f), 2);
            break;
        }
        case FishingComponentState::FishGrabbed: {
            assert(fishingCmp.mTargetFish != INVALID_ENTITY);
            if (wasMousePressed) {
                // Grab fish!
                fishingCmp.mTargetPosition = fishingCmp.mBobberPosition;
               
                FishEcosystem& fishEcosystem = world.getFishEcosystem();
                fishEcosystem.setFishHooked(fishingCmp.mTargetFish, entity);
                FishComponent& fish = registry.get<FishComponent>(fishingCmp.mTargetFish);
                // TODO: Handle NPC and multiplayer as well
                fishingCmp.mIsLocalPlayer = true;
                fishingCmp.mState = FishingComponentState::LocalPlayerMinigame;
                // TODO: Do we need an asset handle?
                const FishDef& fishDef = FishRepository::get().getLoadedOrUnloadedAsset(fish.mFishId);

                // Lock player control
                ++registry.get<PlayerControlComponent>(entity).mInputLockCount;
                mLocalPlayerControlLocked = true;

                UIContext::getInstance().getMinigameContext().beginFishingMinigame(fishDef, mLocalPlayerMinigameGameThreadData.get(), [this, entity, &registry, &world](const FishingMinigameResult& result) {
                    GameThreadTasks::getInstance().addGenericTask([this, entity, result, &registry, &world]() {
                        FishingComponent& fishingCmp = registry.get<FishingComponent>(entity);
                        if (result.result == MinigameResultType::Success) {
                            fishingCmp.mState = FishingComponentState::Success;
                        }
                        else {
                            fishingCmp.mState = FishingComponentState::Fail;
                        }
                        static_assert(e_count(MinigameResultType) == 3);
                    });
                });
                wasMousePressed = false;
            }
            break;
        }
        case FishingComponentState::NPCMinigame: {
            assert(false); // TODO:
            break;
        }
        case FishingComponentState::RemotePlayerMinigame: {
            assert(false); // TODO: MULTIPLAYER
            break;
        }
        case FishingComponentState::LocalPlayerMinigame: {
            wasMousePressed = false; // Never accidentally abort minigame
            f32v2 bobberOffset;
            int tugOfWarValue;
            { // Copy data with minimal critical section
                std::lock_guard lock(mLocalPlayerMinigameGameThreadData->mMutex);
                bobberOffset = mLocalPlayerMinigameGameThreadData->mBobberOffset;
                tugOfWarValue = mLocalPlayerMinigameGameThreadData->mTugOfWarValue;
            }
            const f32 playerAngle = controlCmp.mControllerAngle;
            bobberOffset = MathUtil::rotateVector2DRad(bobberOffset, -playerAngle) * 2.5f;
            // TODO: Ground collision
            const f32 zPos = (-2 + tugOfWarValue) * 0.3f;
            fishingCmp.mBobberPosition = MathUtil::lerpWithDeltaTime(fishingCmp.mBobberPosition, f32v3(fishingCmp.mTargetPosition.x + bobberOffset.x, fishingCmp.mTargetPosition.y + bobberOffset.y, zPos), 0.99, elapsedSec);
            DebugRenderer::drawWireQuadThreadSafe(fishingCmp.mBobberPosition - f32v3(RETICLE_DIMS * 0.5f, RETICLE_DIMS * 0.5f, 0.0f), f32v2(RETICLE_DIMS), color::White, 2);
            break;
        }
        case FishingComponentState::Success: {
            break;
        }
        case FishingComponentState::Fail: {
            break;
        }
        default:
            assert(false);
            break;

    }
    static_assert(e_count(FishingComponentState) == 10);

    if (wasMousePressed) {
        fishingCmp.mState = FishingComponentState::Fail;
    }
}

void FishingComponent::onBobberGrabbed(entt::entity fishEntity) {
    assert(fishEntity == mTargetFish);
    mState = FishingComponentState::FishGrabbed;
}

void FishingComponent::onFishLost() {
    mState = FishingComponentState::Fail;
    mTargetFish = INVALID_ENTITY;
}
