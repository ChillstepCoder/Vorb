#include "stdafx.h"
#include "SimAISystem.h"

#include "world/simulation/host/component/SimComponents.h"
#include "world/simulation/host/component/SettlementComponents.h"

#include "world/simulation/host/HostSimContext.h"
#include "world/simulation/host/SimECS.h"
#include "world/World.h"

#include "math/Random.h"

SimAISystem::SimAISystem(HostSimContext& simContext, SimECS& ecs, entt::registry& registry) :
    mWorld(simContext.getWorld()), mSimContext(simContext), mRegistry(registry), mECS(ecs) {
    mWorldWidthChunks = mWorld.getWidthChunks();
    mEntitiesInChunks.resize(SQ(mWorldWidthChunks));
    
    ecs.registerSimECSListeners(mECSEventListeners);
    ecs.addEntityCreatedListener(mECSEventListeners, [this](SimECSEvent e) {
        SimPositionComponent& p = mRegistry.get<SimPositionComponent>(e.entity);
        mEntitiesInChunks[p.chunk].push_back(e.entity);
    });

    ecs.addEntityDestroyedListener(mECSEventListeners, [this](SimECSEvent e) {
        SimPositionComponent& p = mRegistry.get<SimPositionComponent>(e.entity);
        SimChunkEntityList& entityList = mEntitiesInChunks[p.chunk];
        for (size_t i = 0; i < entityList.size(); ++i) {
            if (entityList[i] == e.entity) {
                entityList[i] = entityList.back();
                entityList.pop_back();
                return;
            }
        }
        panic("Failed to find entity {} of type {} for destroy in SimAISystem", (ui32)e.entity, (ui32)e.type);
    });
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
                const f32v2 newPos = pos.position + f32v2(gen.getRandomFloatSigned() * 15.0f, gen.getRandomFloatSigned() * 15.0f);
                setEntityPosition(entity, newPos);
            }
        }
    }
}

void SimAISystem::setEntityPosition(entt::entity e, f32v2 newPosition) {
    SimPositionComponent& posCmp = mRegistry.get<SimPositionComponent>(e);
    posCmp.position = newPosition;
    ChunkID prevChunk = posCmp.chunk;
    posCmp.chunk = (newPosition.y / CHUNK_WIDTH) * mWorldWidthChunks + newPosition.x / CHUNK_WIDTH;
    if (prevChunk != posCmp.chunk) [[unlikely]] {
        onEntityEnterNewChunk(e, prevChunk, posCmp.chunk);
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
            const f32v2 newPosition = i32v2(glm::round(f32v2(leaderPos.position) + (offsetToTarget / distanceToTarget) * moveDistance));
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

void SimAISystem::onEntityEnterNewChunk(entt::entity entity, ChunkID prevChunk, ChunkID newChunk) {
    PROFILE_FUNCTION();

    SimChunkEntityList& prevEntityList = mEntitiesInChunks[prevChunk];
    bool found = false;
    for (size_t i = 0; i < prevEntityList.size(); ++i) {
        if (prevEntityList[i] == entity) {
            prevEntityList[i] = prevEntityList.back();
            prevEntityList.pop_back();
            found = true;
            break;
        }
    }
    assert(found);
    mEntitiesInChunks[newChunk].emplace_back(entity);
}
