#include "stdafx.h"
#include "SimAISystem.h"

#include "world/simulation/host/component/SimComponents.h"
#include "world/simulation/host/component/SettlementComponents.h"

#include "world/simulation/host/HostSimContext.h"
#include "world/simulation/host/SimECS.h"

#include "math/Random.h"

SimAISystem::SimAISystem(HostSimContext& simContext, SimECS& ecs, entt::registry& registry) :
    mWorld(simContext.getWorld()), mSimContext(simContext), mRegistry(registry), mECS(ecs) {
    mWorldWidthChunks = mWorld.getWidthChunks();
}

void SimAISystem::tick(TimestampMs currentTime, TimestampMs deltaTime) {
    mCurrentTime = currentTime;
    mDeltaTime = deltaTime;

    RandomGenerator& gen = mSimContext.getSimRandomGenerator();
    
    updateCharacterGroups();

  
    { // Update all brains who aren't followers (Complex Logic)
        auto view = mRegistry.view<SimBrainComponent, SimPositionComponent>(entt::exclude<CharacterGroupFollowerComponent>);
        for (auto entity : view) {
            SimBrainComponent& brain = view.get<SimBrainComponent>(entity);
            SimPositionComponent& pos = view.get<SimPositionComponent>(entity);
            if (brain.flags.isBitSet(SimBrainComponentFlags::HasTask)) {
                SimInProgressTaskComponent& task = mRegistry.get<SimInProgressTaskComponent>(entity);
                if (currentTime > task.taskStepEndTime) {
                    handleTaskComplete(entity, brain, task);
                }
            }
            else {
                // TODO: REMOVE
                pos.position.x += gen.getRandomFloatSigned() * 15.0f;
                pos.position.y += gen.getRandomFloatSigned() * 15.0f;
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

    { // Update all followers (Simple Logic)
        auto view = mRegistry.view<SimBrainComponent, SimPositionComponent, CharacterGroupFollowerComponent>();
        for (auto entity : view) {
            SimBrainComponent& brain = view.get<SimBrainComponent>(entity);
            SimPositionComponent& pos = view.get<SimPositionComponent>(entity);
            updateFollowCharacterGroup(entity, brain, pos);
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
    CharacterGroupComponent& groupCmp = mRegistry.get<CharacterGroupComponent>(followCmp.groupEntity);

    if (followCmp.nextFollowCheckTime >= mCurrentTime) {
        // TODO: Check if we should keep following
        followCmp.nextFollowCheckTime = mCurrentTime + CHARACTER_GROUP_DEFAULT_FOLLOW_CHECK_INTERVAL_MS;
    }

    // In sim, we are always just stuck to the leader in a close line regardless of formation, for cheap calculation
    if (pos.updatePosition(groupPosition.getPosition() - groupCmp.currentHeading * (f32)(followCmp.followerIndex * 0.35f), mWorldWidthChunks)) {
        onEntityEnterNewChunk(entity);
    }
}

void SimAISystem::onEntityEnterNewChunk(entt::entity entity) {
    SimPositionComponent& pos = mRegistry.get<SimPositionComponent>(entity);
    ChunkID id = pos.getChunk();
}
