#include "stdafx.h"
#include "FishingComponent.h"

#include "ecs/component/PhysicsComponent.h"
#include "ecs/component/CharacterControlComponent.h"

#include "world/ecosystem/FishEcosystem.h"
#include "world/srv/SrvWorldInterface.h"
#include "debugging/DebugRenderer.h"

#include "world/IWorld.h"
#include "world/IHeightmapGrid.h"
#include "time/GameTimeManager.h"

#include "physics/PhysicsConst.h"

// Initialize launch position
constexpr f32 BOBBER_GRAVITY = GRAVITY_Z;

void FishingComponentSystem::update(IWorld& world, entt::registry& registry) {

    mTimeStep = Services::GameTimeManager::ref().getTimestep();

    auto view = registry.view<FishingComponent, PhysicsComponent, CharacterControlComponent>();

    std::vector<entt::entity> componentsToRemove;

    for (auto entity : view) {
        auto& fishCmp = view.get<FishingComponent>(entity);
        auto& physCmp = view.get<PhysicsComponent>(entity);
        auto& controlCmp = view.get<CharacterControlComponent>(entity);
        updateFishing(world, registry, entity, fishCmp, physCmp, controlCmp);
        if (fishCmp.isDone()) {
            // TODO: Notify inventory of caught fish and such? minigame result?
            // Tell fish we are done
            if (fishCmp.mTargetFish != INVALID_ENTITY) {
                SrvWorldInterface* srvWorld = dynamic_cast<SrvWorldInterface*>(&world);
                if (srvWorld ) {
                    FishEcosystem& fishEcosystem = srvWorld->getFishEcosystem();
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
    fishCmp.mBobberVelocity.x = (castTargetPosition.x - fishCmp.mBobberPosition.x) / totalCastTimeSec;
    fishCmp.mBobberVelocity.y = (castTargetPosition.y - fishCmp.mBobberPosition.y) / totalCastTimeSec;
    // z = z0 + v0*t - 0.5*g*t^2
    fishCmp.mBobberVelocity.z = (castTargetPosition.z - fishCmp.mBobberPosition.z - 0.5 * BOBBER_GRAVITY * SQ(totalCastTimeSec)) / totalCastTimeSec;

    fishCmp.mState = FishingComponentState::Casted;
}

void FishingComponentSystem::updateFishing(IWorld& world, entt::registry& registry, entt::entity entity, FishingComponent& fishCmp, PhysicsComponent& physCmp, CharacterControlComponent& controlCmp) {
    ASSERT_GAME_THREAD();
    // TODO: Configurable
    constexpr f32 RETICLE_DIMS = 0.5f;
    constexpr f32 CASTING_POWER = 0.1f;
    constexpr f32 MIN_CAST_DISTANCE = 1.0f;
    constexpr f32 MAX_CAST_DISTANCE = 10.0f;
    switch (fishCmp.mState) {
        case FishingComponentState::Initializing:
            fishCmp.mState = FishingComponentState::Casting;
            [[fallthrough]];
        case FishingComponentState::Casting: {
            if (fishCmp.mIsCastInputPressed) {
                fishCmp.mCastCharge += CASTING_POWER;
                fishCmp.mCastCharge = glm::min(fishCmp.mCastCharge, MAX_CAST_DISTANCE);
            }
            else if (fishCmp.mCastCharge > MIN_CAST_DISTANCE) {
                castLine(fishCmp, physCmp, controlCmp);
                return;
            } else {
                fishCmp.mState = FishingComponentState::Fail;
                return;
            }

            fishCmp.mTargetPosition = getCastTarget(fishCmp, physCmp, controlCmp);
            DebugRenderer::drawWireQuadThreadSafe(fishCmp.mTargetPosition - f32v3(RETICLE_DIMS * 0.5f, RETICLE_DIMS * 0.5f, 0.0f), f32v2(RETICLE_DIMS), color4(1.0f - fishCmp.mCastCharge / MAX_CAST_DISTANCE, fishCmp.mCastCharge / MAX_CAST_DISTANCE, 0.0f), 2);
            break;
        }
        case FishingComponentState::Casted: {
            fishCmp.mBobberVelocity.z += BOBBER_GRAVITY * mTimeStep;
            fishCmp.mBobberPosition += fishCmp.mBobberVelocity * mTimeStep;
            DebugRenderer::drawWireQuadThreadSafe(fishCmp.mBobberPosition - f32v3(RETICLE_DIMS * 0.5f, RETICLE_DIMS * 0.5f, 0.0f), f32v2(RETICLE_DIMS), color4(1.0f - fishCmp.mCastCharge / MAX_CAST_DISTANCE, fishCmp.mCastCharge / MAX_CAST_DISTANCE, 0.0f), 2);
            // Water impact
            // TODO: True water plane position
            if (fishCmp.mBobberPosition.z <= 0.0f) {
                fishCmp.mBobberPosition.z = 0.0f;
                fishCmp.mBobberVelocity = f32v3(0.0f);
                fishCmp.mState = FishingComponentState::Fishing;
            }
            else if (fishCmp.mBobberPosition.z <= world.getHeightmapGrid().tryComputeHeightAtPoint(f32v2(fishCmp.mBobberPosition))) {
                // Terrain collision is failure
                fishCmp.mState = FishingComponentState::Fail;
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
            fishCmp.mBobberPosition += fishCmp.mBobberVelocity * mTimeStep;
            fishCmp.mBobberVelocity *= BOBBER_DRAG;
            // Reeling
            if (fishCmp.mIsCastInputPressed) {
                f32v2 distanceVec = physCmp.getPosition() - fishCmp.mBobberPosition;
                f32v2 pullNormal = glm::normalize(distanceVec);
                fishCmp.mBobberVelocity += f32v3(pullNormal.x, pullNormal.y, 0.0f) * PULL_ACCELLERATION * mTimeStep;
                const f32 bobberSpeed = glm::length(fishCmp.mBobberVelocity);
                if (bobberSpeed > MAX_PULL_SPEED) {
                    fishCmp.mBobberVelocity == (fishCmp.mBobberVelocity / bobberSpeed) * MAX_PULL_SPEED;
                }
                // Check for getting too close to the player
                if (glm::length2(distanceVec) <= SQ(MIN_CAST_DISTANCE)) {
                    fishCmp.mState = FishingComponentState::Fail;
                    return;
                }
                // Check for beaching the bobber
                const f32 terrainHeight = world.getHeightmapGrid().tryComputeHeightAtPoint(f32v2(fishCmp.mBobberPosition));
                // TODO: If tile handle is invalid, switch to LOD?
                if (terrainHeight == FLT_MAX) {
                    fishCmp.mState = FishingComponentState::Fail;
                    return;
                }
                if (terrainHeight >= -0.01f) {
                    fishCmp.mState = FishingComponentState::Fail;
                    return;
                }
            }

            // TODO Server version
            SrvWorldInterface* srvWorld = dynamic_cast<SrvWorldInterface*>(&world);
            if (srvWorld && fishCmp.mTargetFish == INVALID_ENTITY) {
                FishEcosystem& fishEcosystem = srvWorld->getFishEcosystem();
                PreciseTimer timer;
                entt::entity closestFish = fishEcosystem.getClosestIdleFishToPoint(fishCmp.mBobberPosition, FISH_ATTRACT_DISTANCE);
                if (closestFish != INVALID_ENTITY) {
                    fishCmp.mTargetFish = closestFish;
                    fishEcosystem.setFishFollowTarget(closestFish, entity);
                }
                LOG_WARN("CLOSEST FOUND IN {} ms", timer.stop());
                return;
            }

            DebugRenderer::drawWireQuadThreadSafe(fishCmp.mBobberPosition - f32v3(RETICLE_DIMS * 0.5f, RETICLE_DIMS * 0.5f, 0.0f), f32v2(RETICLE_DIMS), color4(1.0f - fishCmp.mCastCharge / MAX_CAST_DISTANCE, fishCmp.mCastCharge / MAX_CAST_DISTANCE, 0.0f), 2);
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
            break;
        }
        default:
            assert(false);
            break;

    }
    static_assert(e_count(FishingComponentState) == 9);
}
