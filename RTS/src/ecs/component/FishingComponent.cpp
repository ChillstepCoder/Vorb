#include "stdafx.h"
#include "FishingComponent.h"

#include "ecs/component/PhysicsComponent.h"
#include "ecs/component/CharacterControlComponent.h"

#include "debugging/DebugRenderer.h"

void FishingComponentSystem::update(IWorld& world, entt::registry& registry) {
    auto view = registry.view<FishingComponent, PhysicsComponent, CharacterControlComponent>();

    std::vector<entt::entity> componentsToRemove;

    for (auto entity : view) {
        auto& fishCmp = view.get<FishingComponent>(entity);
        auto& physCmp = view.get<PhysicsComponent>(entity);
        auto& controlCmp = view.get<CharacterControlComponent>(entity);
        updateFishing(world, registry, fishCmp, physCmp, controlCmp);
        if (fishCmp.isDone()) {
            // TODO: Notify inventory of caught fish and such? minigame result?
            componentsToRemove.emplace_back(entity);
        }
    }

    for (auto&& entity : componentsToRemove) {
        registry.remove<FishingComponent>(entity);
    }
}

void FishingComponentSystem::updateFishing(IWorld& world, entt::registry& registry, FishingComponent& fishCmp, PhysicsComponent& physCmp, CharacterControlComponent& controlCmp) {
    // TODO: Configurable
    constexpr f32 CASTING_POWER = 0.1f;
    constexpr f32 MIN_CAST_POWER = 1.0f;
    constexpr f32 MAX_CAST_DISTANCE = 10.0f;
    switch (fishCmp.mState) {
        case FishingComponentState::Casting: {
            if (fishCmp.mIsCastInputPressed) {
                fishCmp.mCastCharge += CASTING_POWER;
                fishCmp.mCastCharge = glm::min(fishCmp.mCastCharge, MAX_CAST_DISTANCE);
            }
            else if (fishCmp.mCastCharge >= MIN_CAST_POWER) {
                fishCmp.mState = FishingComponentState::Fail;
                return;
            } else {
                fishCmp.mState = FishingComponentState::Fail;
                return;
            }

            constexpr f32 RETICLE_DIMS = 0.5f;
            const f32v2 controllerDir = controlCmp.getControllerDir();
            f32v3 castTarget = physCmp.getPosition();
            castTarget.z = 0.0f;
            castTarget.x += controllerDir.x * fishCmp.mCastCharge;
            castTarget.y += controllerDir.y * fishCmp.mCastCharge;
            DebugRenderer::drawWireQuadThreadSafe(castTarget - f32v3(RETICLE_DIMS * 0.5f, RETICLE_DIMS * 0.5f, 0.0f), f32v2(RETICLE_DIMS), color4(1.0f - fishCmp.mCastCharge / MAX_CAST_DISTANCE, fishCmp.mCastCharge / MAX_CAST_DISTANCE, 0.0f), 2);
            break;
        }
        case FishingComponentState::Fishing: {
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
            break;

    }
    static_assert(e_count(FishingComponentState) == 7);
}
