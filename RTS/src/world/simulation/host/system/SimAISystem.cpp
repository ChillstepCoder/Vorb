#include "stdafx.h"
#include "SimAISystem.h"

#include "world/simulation/host/component/SimComponents.h"
#include "world/simulation/host/component/SettlementComponents.h"

#include "world/simulation/host/HostSimContext.h"
#include "world/simulation/host/SimECS.h"


SimAISystem::SimAISystem(HostSimContext& simContext, entt::registry& registry) : 
    mSimContext(simContext), mRegistry(registry), mECS(simContext.getECS()) {

}

void SimAISystem::tick(TimestampMs currentTime, TimestampMs deltaTime) {
    mCurrentTime = currentTime;
    mDeltaTime = deltaTime;

    // Update all brains
    auto view = mRegistry.view<SimBrainComponent, SimPositionComponent>();
    LOG_TRACE("BRAIN COUNT %d", view.size_hint());
    for (auto entity : view) {
        SimBrainComponent& brain = view.get<SimBrainComponent>(entity);
        SimPositionComponent& pos = view.get<SimPositionComponent>(entity);
        if (brain.flags.isBitSet(SimBrainComponentFlags::IsFollowingCharacterGroup)) {
            updateFollowCharacterGroup(entity, brain, pos);
        }
        else if (brain.flags.isBitSet(SimBrainComponentFlags::HasTask)) {
            SimInProgressTaskComponent& task = mRegistry.get<SimInProgressTaskComponent>(entity);
            if (currentTime > task.taskStepEndTime) {
                handleTaskComplete(entity, brain, task);
            }
        }
    }
}


void SimAISystem::updateCharacterGroups() {
    std::vector<entt::entity> groupsToEnd;
    // Update all character groups
    auto viewGroup = mRegistry.view<CharacterGroupComponent, SimPositionComponent>();
    for (auto entity : viewGroup) {
        CharacterGroupComponent& group = viewGroup.get<CharacterGroupComponent>(entity);
        SimPositionComponent& pos = viewGroup.get<SimPositionComponent>(entity);
        if (mCurrentTime >= group.nextRefreshTime) {
            // TODO: REFRESH LOGIC
            group.nextRefreshTime = mCurrentTime + CHARACTER_GROUP_DEFAULT_REFRESH_INTERVAL_MS;
        }
        // Puppeteer the leader
        if (group.leader != entt::null) [[likely]] {
            SimPositionComponent& leaderPos = mRegistry.get<SimPositionComponent>(group.leader);

            // TODO: Group type specific logic
            const f32v2 offsetToTarget = f32v2(group.targetPos) - f32v2(leaderPos.position);
            const f32 distanceToTarget = glm::length(offsetToTarget);
            constexpr f32 minMoveStep = 2.f; // Prevents getting stuck at one tile due to small move increments
            constexpr f32 COMPLETE_DISTANCE = 8.f;
            
            const f32 moveDistance = glm::min(distanceToTarget, glm::max(group.moveSpeed * (mDeltaTime / MS_PER_SECOND), minMoveStep));
            leaderPos.position = i32v2(glm::round(f32v2(leaderPos.position) + (offsetToTarget / distanceToTarget) * moveDistance));
            pos.position = leaderPos.position;

            if (distanceToTarget - moveDistance <= COMPLETE_DISTANCE) {
                groupsToEnd.push_back(entity);
            }
        }
        else {
            panic("Character group has no leader!");
        }
    }

    // TODO: Only end group if its the type that wants to end on reach target!
    for (entt::entity group : groupsToEnd) {
        mECS.endCharacterGroup(group, CharacterGroupDissolveReason::GoalSuccess);
    };
}

void SimAISystem::handleTaskComplete(entt::entity entity, SimBrainComponent& brain, SimInProgressTaskComponent& taskCmp) {
    assert(false);
}

void SimAISystem::updateFollowCharacterGroup(entt::entity entity, SimBrainComponent& brain, SimPositionComponent& pos) {
    // Snap to leader position
    CharacterGroupFollowerComponent& followCmp = mRegistry.get<CharacterGroupFollowerComponent>(entity);
    SimPositionComponent& groupPosition = mRegistry.get<SimPositionComponent>(followCmp.groupEntity);
    pos.position = groupPosition.position;
}