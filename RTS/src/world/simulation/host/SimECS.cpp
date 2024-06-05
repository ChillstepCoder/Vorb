#include "stdafx.h"
#include "SimECS.h"

#include "world/simulation/host/HostSimContext.h"
#include "world/simulation/host/component/SimCharacterComponents.h"
#include "world/simulation/host/component/SimSettlementComponents.h"
#include "world/simulation/host/system/SimAISystem.h"
#include "world/simulation/host/system/SimSettlementSystem.h"
#include "world/simulation/host/SimThread.h"
#include "ecs/IEntityComponentSystem.h"
#include "ecs/component/FullEntityBindingComponent.h"
#include "gamethread/GameThreadTasks.h"
#include "world/IChunkGrid.h"

#include "text/NameManager.h"

#include "math/Random.h"

#include "world/World.h"

#include "options/DebugOptions.h"
#include "debugging/DebugRenderer.h"

SimECS::SimECS(HostSimContext& hostSimContext) :
    mHostSimContext(hostSimContext), mWorld(hostSimContext.getWorld()), mEntityTransitioner(hostSimContext.getEntityTransitionManager()
) {
    mAISystem = std::make_unique<SimAISystem>(hostSimContext, *this, mRegistry);
    mSettlementSystem = std::make_unique<SimSettlementSystem>(hostSimContext, *this, mRegistry);
    mEntitiesInChunks.resize(mWorld.getTotalChunks());
    // Prevent allocations
    for (auto& list : mEntitiesInChunks) {
        list.reserve(ENTITY_LIST_RESERVE_COUNT);
    }
}

SimECS::~SimECS() {

}

void SimECS::tickSimThread(TimestampMs currentTimestamp) {
    ASSERT_SIM_THREAD();
    PROFILE_FUNCTION();

    mDebugDrawAgents[1].clear();

    mCurrentTickTimestamp = currentTimestamp;
    mTimeDelta = currentTimestamp - mLastTickTimestamp;
    mLastTickTimestamp = currentTimestamp;
    
    mAISystem->tick(mCurrentTickTimestamp, mTimeDelta);
    mSettlementSystem->tick(mCurrentTickTimestamp, mTimeDelta);

    {
        std::lock_guard lock(mDebugDrawMutex);
        mDebugDrawAgents[1].swap(mDebugDrawAgents[0]);
    }
    debugRenderInternal();
}

entt::entity SimECS::createNewPerson(f32v2 worldTilePosition) {
    ASSERT_SIM_THREAD();

    entt::entity newPerson = mRegistry.create();
    RandomGenerator& gen = mHostSimContext.getSimRandomGenerator();

    const bool isFemale = gen.getRandomBool();

    // Sim components
    SimPositionComponent& posCmp = mRegistry.emplace<SimPositionComponent>(newPerson, worldTilePosition, mWorld.getChunkIDAtWorldPos(worldTilePosition));
    mRegistry.emplace<SimMovementComponent>(newPerson);
    mRegistry.emplace<SimBrainComponent>(newPerson);
    mRegistry.emplace<SimNeedsComponent>(newPerson);
    mRegistry.emplace<SimProfessionComponent>(newPerson);

    // Dual components
    mRegistry.emplace<DualCharacterComponent>(newPerson, ++mUIDGenerator);
    mRegistry.emplace<DualGenderComponent>(newPerson, isFemale);
    mRegistry.emplace<DualTaskQueueComponent>(newPerson);
    mRegistry.emplace<DualInventoryComponent>(newPerson);
    mRegistry.emplace<DualAttributesComponent>(newPerson).init(
        DEFAULT_HEALTH,
        DEFAULT_STAMINA,
        DEFAULT_BLOOD,
        DEFAULT_MOVE_SPEED
    );
    mRegistry.emplace<SimEntityTypeComponent>(newPerson).type = SimEntityType::Character;

    SimCharacterNameComponent& nameCmp = mRegistry.emplace<SimCharacterNameComponent>(newPerson);
    nameCmp.firstName = NameManager::getRandomFirstName(gen, isFemale);
    nameCmp.lastName = NameManager::getRandomLastName(gen);

    mEntitiesInChunks[posCmp.chunk].push_back(newPerson);

    dispatchEntityCreated(SimECSEvent{ newPerson, SimEntityType::Character });

    return newPerson;
}


entt::entity SimECS::createNewSettlerCaravan(std::span<entt::entity> members, int leaderIndex, ChunkID targetChunk) {
    entt::entity groupEntity = createNewCharacterGroup(members, leaderIndex, CharacterGroupType::SettlerCaravan);
    CharacterGroupComponent& groupCmp = mRegistry.get<CharacterGroupComponent>(groupEntity);
    const f32v2 targetPos = GridIdUtil::getWorldPosCenter(targetChunk, CHUNK_WIDTH, mWorld.getWidthChunks());
    groupCmp.targetPos = targetPos;
    groupCmp.targetChunk = targetChunk;

    const f32v2 offsetToTarget = targetPos - mRegistry.get<SimPositionComponent>(groupCmp.leader).position;
    if (offsetToTarget != f32v2(0.0f)) {
        groupCmp.currentHeading = glm::normalize(offsetToTarget);
    }

    dispatchEntityCreated(SimECSEvent{ groupEntity, SimEntityType::Group });
    return groupEntity;
}

void SimECS::endCharacterGroup(entt::entity group, CharacterGroupDissolveReason reason) {
    CharacterGroupComponent& groupCmp = mRegistry.get<CharacterGroupComponent>(group);
    assert(groupCmp.leader != entt::null);

    // Resolve group end
    switch (groupCmp.groupType) {
        case CharacterGroupType::Generic:
            break;
        case CharacterGroupType::SettlerCaravan: {
            const bool didCreate = mSettlementSystem->tryCreateSettlementFromGroup(group);
            if (!didCreate) {
                LOG_CRITICAL("TODO: SETTLERS MUST FIND NEW SETTLE TARGET!");
            }
            break;
        }
        case CharacterGroupType::Combat:
            break;
        case CharacterGroupType::TradeCaravan:
            assert(false);
            break;
        default:
            assert(false);

    }
    static_assert(e_count(CharacterGroupType) == 4, "Add resolve if needed");

    for (auto& follower : groupCmp.groupMembers) {
        CharacterGroupFollowerComponent& followerCmp = mRegistry.get<CharacterGroupFollowerComponent>(follower);

        // Resolve character end
        if (reason == CharacterGroupDissolveReason::GoalSuccess) {
            switch (followerCmp.followReason) {
                case CharacterGroupFollowerReason::None:
                    break;
                case CharacterGroupFollowerReason::Settler:
                    break;
                case CharacterGroupFollowerReason::Bodyguard:
                    break;
                default:
                    assert(false);

            }
            static_assert(e_count(CharacterGroupFollowerReason) == 3, "Add resolve if needed");
        }

        mRegistry.remove<CharacterGroupFollowerComponent>(follower);
    }

    onEntityDestroyed(group, SimEntityType::Group);
    mRegistry.remove<CharacterGroupLeaderComponent>(groupCmp.leader);
    mRegistry.destroy(group);
}

void SimECS::simThreadOnActivateChunk(ChunkID id) {
    ASSERT_SIM_THREAD();
    PROFILE_FUNCTION();
    mEntityTransitioner.markChunkSimEntitiesForTransition(id);
}

bool SimECS::onEntityEnterNewChunk(entt::entity entity, ChunkID prevChunk, ChunkID newChunk) {
    ASSERT_SIM_THREAD();
    PROFILE_FUNCTION();

    EntityVector& prevEntityList = mEntitiesInChunks[prevChunk];
    bool found = false;
    // We amortize this by reverse iterating as
    // more dynamic entities are likely to be at the end of the list
    for (int i = (int)prevEntityList.size() - 1; i >= 0; --i) {
        if (prevEntityList[i] == entity) {
            prevEntityList[i] = prevEntityList.back();
            prevEntityList.pop_back();
            // Free memory when needed
            if (prevEntityList.capacity() > prevEntityList.size() + ENTITY_LIST_RESERVE_COUNT) [[unlikely]] {
                prevEntityList.shrink_to_fit();
                prevEntityList.reserve(ENTITY_LIST_RESERVE_COUNT);
            }
            found = true;
            break;
        }
    }
    assert(found);

    if (mHostSimContext.isChunkSimulating(newChunk)) {
        mEntitiesInChunks[newChunk].emplace_back(entity);
        return false;
    }
    else {
        mEntityTransitioner.markSimEntityForTransition(entity);
        return true;
    }
}

void SimECS::debugRender(f32v3 cameraPos) const {
    ASSERT_RENDER_THREAD();
    if (sDebugOptions.mShowSettlementDebug) {
        std::lock_guard lock(mDebugRenderMutex);
        mDebugCameraPos = cameraPos;
    }
    else {
        std::lock_guard lock(mDebugRenderMutex);
        mDebugCameraPos.x = FLT_MAX;
    }

    // Simulation character debug
    if (sDebugOptions.mDebugSimCharacters) { // NOT THREAD SAFE BOOL ACCESS
        std::vector<DebugDrawSimAgentData> drawAgents;
        {
            std::lock_guard lock(mDebugDrawMutex);
            drawAgents = mDebugDrawAgents[0];
        }
        for (DebugDrawSimAgentData drawData : drawAgents) {
            DebugRenderer::drawWireQuad(drawData.pos - f32v3(0.4f, 0.4f, 0.0f), f32v2(0.8f), drawData.color);
        }
    }
}

void SimECS::debugRenderInternal() const {
    f32v3 cameraPos;
    { // Critical section
        std::lock_guard lock(mDebugRenderMutex);
        cameraPos = mDebugCameraPos;
    }
    if (cameraPos.x == FLT_MAX) {
        return;
    }

    // Draw closest one
    auto view = mRegistry.view<const SettlementSimComponent, const SettlementLayoutComponent>();
    f32 closestDistSq = FLT_MAX;
    const SettlementLayoutComponent* closestLayoutCmp = nullptr;
    for (entt::entity entity : view) {
        const SettlementSimComponent& settlementCmp = view.get<const SettlementSimComponent>(entity);
        const SettlementLayoutComponent& layoutCmp = view.get<const SettlementLayoutComponent>(entity);
        f32v2 worldPos = mWorld.getChunkWorldPos(settlementCmp.rootChunkId).v;
        f32 distSq = glm::distance2(f32v2(cameraPos), worldPos);
        if (distSq < closestDistSq) {
            closestDistSq = distSq;
            closestLayoutCmp = &layoutCmp;
        }
    }
    if (closestLayoutCmp) {
        closestLayoutCmp->manager.debugDraw();
    }

}

entt::entity SimECS::createNewCharacterGroup(std::span<entt::entity> members, int leaderIndex, CharacterGroupType groupType) {
    assert(leaderIndex < members.size());

    entt::entity leader = members[leaderIndex];
    CharacterGroupLeaderComponent& leaderCmp = mRegistry.get_or_emplace<CharacterGroupLeaderComponent>(leader);
    mRegistry.get<SimBrainComponent>(members[leaderIndex]).flags.setBit(SimBrainComponentFlags::IsCharacterGroupLeader);

    entt::entity groupEntity = mRegistry.create();
    CharacterGroupComponent& groupCmp = mRegistry.emplace<CharacterGroupComponent>(groupEntity);

    // Start at the leaders position
    const SimPositionComponent& leaderPos = mRegistry.get<SimPositionComponent>(leader);
    mRegistry.emplace<SimPositionComponent>(groupEntity, leaderPos.position, leaderPos.chunk);
    mRegistry.emplace<SimEntityTypeComponent>(groupEntity).type = SimEntityType::Group;
    groupCmp.leader = leader;
    groupCmp.groupType = groupType;

    // Register all members and track average movement speed of the caravan from their move speeds
    f32 avgMoveSpeed = 0.0f;
    groupCmp.groupMembers.reserve(members.size());
    for (size_t i = 0; i < members.size(); ++i) {
        groupCmp.groupMembers.push_back(members[i]);
        CharacterGroupFollowerComponent& followerCmp = mRegistry.get_or_emplace<CharacterGroupFollowerComponent>(members[i]);
        followerCmp.groupEntity = groupEntity;
        followerCmp.nextFollowCheckTime = mCurrentTickTimestamp + CHARACTER_GROUP_DEFAULT_FOLLOW_CHECK_INTERVAL_MS;
        followerCmp.followerIndex = i;
        mRegistry.get<SimBrainComponent>(members[i]).flags.setBit(SimBrainComponentFlags::IsFollowingCharacterGroup);
        avgMoveSpeed += mRegistry.get<DualAttributesComponent>(members[i]).getCurrentAttribute(AttributeType::MoveSpeed);
    }
    avgMoveSpeed = glm::max(avgMoveSpeed, 0.1f);
    avgMoveSpeed /= members.size();
    groupCmp.moveSpeed = avgMoveSpeed;
    groupCmp.nextRefreshTime = mCurrentTickTimestamp + CHARACTER_GROUP_DEFAULT_REFRESH_INTERVAL_MS;

    mEntitiesInChunks[leaderPos.chunk].push_back(groupEntity);

    return groupEntity;
}

void SimECS::onEntityDestroyed(entt::entity entity, SimEntityType type) {
    SimPositionComponent& p = mRegistry.get<SimPositionComponent>(entity);
    EntityVector& entityList = mEntitiesInChunks[p.chunk];

    assert(mRegistry.get<SimEntityTypeComponent>(entity).type == type);

    for (size_t i = 0; i < entityList.size(); ++i) {
        if (entityList[i] == entity) {
            entityList[i] = entityList.back();
            entityList.pop_back();
            return;
        }
    }
    panic("Failed to find entity {} of type {} for destroy in SimAISystem", (ui32)entity, (ui32)type);

    dispatchEntityDestroyed(SimECSEvent{ entity, type });
}
