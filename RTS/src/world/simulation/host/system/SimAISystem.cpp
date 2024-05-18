#include "stdafx.h"
#include "SimAISystem.h"

#include "world/simulation/host/component/SimCharacterComponents.h"
#include "world/simulation/host/component/SimSettlementComponents.h"

#include "world/simulation/host/HostSimContext.h"
#include "world/simulation/host/SimECS.h"
#include "world/simulation/host/system/SimSettlementSystem.h"
#include "world/simulation/host/settlement/SimSettlementCharacterInterface.h"
#include "world/World.h"

#include "math/Random.h"

#include "debugging/DebugRenderer.h"

SimAISystem::SimAISystem(HostSimContext& simContext, SimECS& ecs, entt::registry& registry) :
    mWorld(simContext.getWorld()), mSimContext(simContext), mRegistry(registry), mECS(ecs) {
    mWorldWidthChunks = mWorld.getWidthChunks();
    mWorldWidthTiles = mWorld.getWidthTiles();
}

void SimAISystem::tick(TimestampMs currentTime, TimestampMs deltaTimeMs) {
    mCurrentTime = currentTime;
    mDeltaTimeMs = deltaTimeMs;
    mDeltaTimeSec = deltaTimeMs / MS_PER_SECOND;
    mRandomGen = &mSimContext.getSimRandomGenerator();
    
    updateCharacterGroups();
  
    { // Update all brains who aren't followers (Complex Logic)
        auto view = mRegistry.view<SimBrainComponent, SimPositionComponent, SimMovementComponent>(entt::exclude<CharacterGroupFollowerComponent>);
        for (auto entity : view) {
            SimBrainComponent& brain = view.get<SimBrainComponent>(entity);
            SimPositionComponent& pos = view.get<SimPositionComponent>(entity);
            SimMovementComponent& movement = view.get<SimMovementComponent>(entity);
            updateSimCharacter(brain, pos, movement, entity);
        }
    }
}

bool SimAISystem::setEntityPosition(entt::entity e, f32v2 newPosition) {
    newPosition = glm::clamp(newPosition, 0.0f, mWorldWidthTiles - 1.0f);

    SimPositionComponent& posCmp = mRegistry.get<SimPositionComponent>(e);
    posCmp.position = newPosition;
    ChunkID prevChunk = posCmp.chunk;
    posCmp.chunk = ui32(newPosition.y / CHUNK_WIDTH) * mWorldWidthChunks + ui32(newPosition.x / CHUNK_WIDTH);
    assert(posCmp.chunk == mWorld.getChunkIDAtWorldPos(posCmp.position));
    if (prevChunk != posCmp.chunk) [[unlikely]] {
        return mECS.onEntityEnterNewChunk(e, prevChunk, posCmp.chunk);
    }
    return false;
}

FamilyID SimAISystem::createFamily(std::span<entt::entity> entities, const char* name) {
    ASSERT_SIM_THREAD();
    // TODO: Allow rollover on live, check for duplicate family IDs to be safe
    assert(mFamilyIDGen < INVALID_FAMILY_ID);
    SimFamily& newFamily = mFamilies[++mFamilyIDGen];
    newFamily.characters = std::make_unique<entt::entity[]>(entities.size());
    for (size_t i = 0; i < entities.size(); ++i) {
        newFamily.characters[i] = entities[i];
        mRegistry.emplace<SimFamilyMemberComponent>(entities[i], mFamilyIDGen);
    }
    memcpy(newFamily.characters.get(), entities.data(), entities.size_bytes());
    newFamily.numCharacters = entities.size();
    newFamily.name = name;
    return mFamilyIDGen;
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
            
            const f32 moveDistance = glm::min(distanceToTarget, glm::max(group.moveSpeed * mDeltaTimeSec, minMoveStep));
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

void SimAISystem::updateSimCharacter(SimBrainComponent& brain, SimPositionComponent& pos, SimMovementComponent& movement, entt::entity entity) {

    // Handle move orders
    if (movement.targetPosition.x >= 0.0f) {
        const f32 MOVE_SPEED = mDeltaTimeSec * 2.0f;
        const f32v2 offsetToTarget = f32v2(movement.targetPosition) - pos.position;
        const f32 distanceToTarget = glm::length(offsetToTarget);
        if (distanceToTarget < MOVE_SPEED) {
            if (setEntityPosition(entity, movement.targetPosition)) {
                // We are now activating
                return;
            }
            movement.targetPosition = f32v2(-1.0f);
        }
        else {
            f32v2 newPosition = pos.position + (offsetToTarget / distanceToTarget) * MOVE_SPEED;
            if (setEntityPosition(entity, newPosition)) {
                // We are now activating
                return;
            }
        }
    }

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

        // Aquire or prioritize shelter if needed
        color4 debugColor = color4(1.0f, 0.0f, 1.0f, 1.0f);
        switch (residencyCmp->homeState) {
            case SimHomeState::Homeless:
                mECS.getSettlementSystem().getCharacterInterface().tryRequestHomeForSelfAndFamily(entity, residencyCmp->settlementEntity);
                debugColor = color4(1.0f, 0.0f, 0.0f, 1.0f);
                break;
            case SimHomeState::Pending:
                debugColor = color4(1.0f, 1.0f, 0.0f, 1.0f);
                // Waiting for the settlement to gift us a plot
                break;
            case SimHomeState::Building:
                debugColor = color4(0.0f, 1.0f, 1.0f, 1.0f);
                // TODO: Aquire building task
                break;
            case SimHomeState::Done:
                debugColor = color4(1.0f, 1.0f, 1.0f, 1.0f);
                break;
            default:
                break;
        }
        // Try aquire task

        // Select wander target around home point if not moving
        if (movement.targetPosition.x < 0.0f && residencyCmp->homePoint.x > -1) {
            constexpr f32 MAX_WANDER_RADIUS = 256.0f;
            constexpr f32 MAX_WANDER_RADIUS_SQ = SQ(MAX_WANDER_RADIUS);
            // Acquire new wander position
            const f32 randRadius = mRandomGen->getRandomFloatUnsigned() * MAX_WANDER_RADIUS;
            const f32 randRotation = mRandomGen->getRandomFloatUnsigned() * M_2_PIF;
            const f32v2 offset(randRadius * cosf(randRotation), randRadius * sinf(randRotation));
            movement.targetPosition = f32v2(residencyCmp->homePoint) + offset;
        }

        DebugRenderer::drawWireQuadThreadSafe(f32v3(pos.position.x, pos.position.y, 5.0f), f32v2(1.0f), debugColor, 30);
    }
}
