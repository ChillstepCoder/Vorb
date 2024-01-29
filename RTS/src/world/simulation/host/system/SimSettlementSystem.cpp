#include "stdafx.h"
#include "SimSettlementSystem.h"

#include "world/simulation/host/component/SimComponents.h"
#include "world/simulation/host/component/SettlementComponents.h"

#include "text/NameManager.h"

SimSettlementSystem::SimSettlementSystem(HostSimContext& simContext, SimECS& ecs, entt::registry& registry) :
    mSimContext(simContext), mRegistry(registry), mECS(ecs) {
}

void SimSettlementSystem::tick(TimestampMs currentTime, TimestampMs deltaTime) {
    mCurrentTime = currentTime;
    mDeltaTime = deltaTime;

    //RandomGenerator& gen = mSimContext.getSimRandomGenerator();
}

void SimSettlementSystem::createSettlementFromGroup(entt::entity groupEntity) {
    CharacterGroupComponent& groupCmp = mRegistry.get<CharacterGroupComponent>(groupEntity);
    assert(groupCmp.leader != entt::null);
}
