#include "stdafx.h"
#include "SimAISystem.h"

#include "world/simulation/host/component/SimCharacterComponents.h"
#include "world/simulation/host/component/SimSettlementComponents.h"

#include "world/simulation/host/HostSimContext.h"
#include "world/simulation/host/SimECS.h"
#include "world/simulation/host/settlement/SimSettlementCharacterInterface.h"
#include "world/World.h"

#include "math/Random.h"

#include "debugging/DebugRenderer.h"

SimAISystem::SimAISystem(HostSimContext& simContext, SimECS& ecs, entt::registry& registry) :
    mWorld(simContext.getWorld()), mSimContext(simContext), mRegistry(registry), mECS(ecs) {
    mWorldWidthChunks = mWorld.getWidthChunks();
}

void SimAISystem::tick(TimestampMs currentTime, TimestampMs deltaTime) {
    mCurrentTime = currentTime;
    mDeltaTime = deltaTime;
    mRandomGen = &mSimContext.getSimRandomGenerator();
    
    updateCharacterGroups();
  
    { // Update all brains who aren't followers (Complex Logic)
        auto view = mRegistry.view<SimBrainComponent, SimPositionComponent>(entt::exclude<CharacterGroupFollowerComponent>);
        for (auto entity : view) {
            SimBrainComponent& brain = view.get<SimBrainComponent>(entity);
            SimPositionComponent& pos = view.get<SimPositionComponent>(entity);
            updateSimBrain(brain, pos, entity);
        }
    }
}

void SimAISystem::setEntityPosition(entt::entity e, f32v2 newPosition) {

    // TODO: FIX
    newPosition = glm::clamp(newPosition, 0.0f, 32767.f);

    SimPositionComponent& posCmp = mRegistry.get<SimPositionComponent>(e);
    posCmp.position = newPosition;
    ChunkID prevChunk = posCmp.chunk;
    posCmp.chunk = ui32(newPosition.y / CHUNK_WIDTH) * mWorldWidthChunks + ui32(newPosition.x / CHUNK_WIDTH);
    if (prevChunk != posCmp.chunk) [[unlikely]] {
        mECS.onEntityEnterNewChunk(e, prevChunk, posCmp.chunk);
    }
}

void SimAISystem::updateCharacterGroups() {
    std::vector<entt::entity> groupsToEnd;
    // Update all character groups
    auto viewGroup = mRegistry.view<CharacterGroupComponent, SimPositionComponent>();
    for (auto groupEntity : viewGroup) {
        CharacterGroupComponent& group = viewGroup.get<CharacterGroupComponent>(groupEntity);
        SimPositionComponent& pos = viewGroup.get<SimPositionComponent>(groupEntity);
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
            f32v2 newPosition;
            if (distanceToTarget < 0.0001f) [[unlikely]] {
                newPosition = group.targetPos;
            }
            else {
                newPosition = i32v2(glm::round(f32v2(leaderPos.position) + (offsetToTarget / distanceToTarget) * moveDistance));
            }
            setEntityPosition(group.leader, newPosition);
            setEntityPosition(groupEntity, newPosition);

            if (distanceToTarget - moveDistance <= COMPLETE_DISTANCE) {
                groupsToEnd.push_back(groupEntity);
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
    setEntityPosition(entity, groupPosition.getPosition() - groupCmp.currentHeading * (f32)(followCmp.followerIndex * 0.35f));
}

void SimAISystem::updateSimBrain(SimBrainComponent& brain, SimPositionComponent& pos, entt::entity entity) {
    if (brain.flags.isBitSet(SimBrainComponentFlags::HasTask)) {
        SimInProgressTaskComponent& task = mRegistry.get<SimInProgressTaskComponent>(entity);
        if (mCurrentTime > task.taskStepEndTime) {
            handleTaskComplete(entity, brain, task);
        }
    }
    else {
        SimResidentComponent* residencyCmp = mRegistry.try_get<SimResidentComponent>(entity);
        assert(residencyCmp); // TODO: Handle nomadic people or those who need to find residency
        assert(residencyCmp->settlementEntity != entt::null);

        // Try aquire residency
        if (residencyCmp->homeId == INVALID_BUILDING_ID) {
            if (!residencyCmp->flags.isBitSet(SimResidentComponentFlags::HasPendingHome)) {
                SimSettlementCharacterInterface::tryRequestHome(entity, residencyCmp->settlementEntity, mRegistry);
            }
        }
        // Try aquire task

        // Wander if failed to aquire task
        

        // TODO: REMOVE
        const f32 WANDER_SPEED = mDeltaTime * 2.0f;
        const f32v2 newPos = pos.position + f32v2(mRandomGen->getRandomFloatSigned() * WANDER_SPEED, mRandomGen->getRandomFloatSigned() * WANDER_SPEED);
        setEntityPosition(entity, newPos);
        DebugRenderer::drawWireQuadThreadSafe(f32v3(newPos.x, newPos.y, 5.0f), f32v2(1.0f), color4(1.0f, 0.0f, 1.0f, 1.0f), 30);
    }
}
