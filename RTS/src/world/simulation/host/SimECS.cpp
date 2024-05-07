#include "stdafx.h"
#include "SimECS.h"

#include "world/simulation/host/HostSimContext.h"
#include "world/simulation/host/component/SimComponents.h"
#include "world/simulation/host/component/SettlementComponents.h"

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


constexpr ui32 ENTITY_LIST_RESERVE_COUNT = 64;
// Prevent lists getting too out of control
constexpr ui32 ENTITY_LIST_DEALLOCATE_COUNT = 512;

SimECS::SimECS(HostSimContext& hostSimContext) : mHostSimContext(hostSimContext), mWorld(hostSimContext.getWorld()) {
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

    mCurrentTickTimestamp = currentTimestamp;
    mTimeDelta = currentTimestamp - mLastTickTimestamp;
    mLastTickTimestamp = currentTimestamp;
    
    mAISystem->tick(mCurrentTickTimestamp, mTimeDelta);
    mSettlementSystem->tick(mCurrentTickTimestamp, mTimeDelta);

    // Send newly activated entities to game thread
    if (mFullActivatedEntitiesThisFrame.size()) {
        for (auto& [chunkId, entities] : mFullActivatedEntitiesThisFrame) {
            GameThreadTasks::getInstance().addGenericTask([this, chunkId, entities = std::move(entities)]() mutable {
                Chunk& chunk = mWorld.getChunkGrid().getChunk(chunkId);
                if (chunk.isActivated()) {
                    mWorld.getECS().createFullEntitiesFromSimEntities(mWorld.getChunkGrid().getChunk(chunkId), entities);
                }
                else {
                    // Rare case where chunk deactivated when we were trying to send it entities, so we need to send them back
                    mHostSimContext.tryGetSimThread()->addTask([this, chunkId, entities = std::move(entities)]() mutable {
                        onEntityFullActivationFailed(chunkId, std::move(entities));
                    });
                }
            });
        }
    }
    mFullActivatedEntitiesThisFrame.clear();

    debugRenderInternal();
}

entt::entity SimECS::createNewPerson(f32v2 worldTilePosition) {
    ASSERT_SIM_THREAD();

    entt::entity newPerson = mRegistry.create();
    RandomGenerator& gen = mHostSimContext.getSimRandomGenerator();

    const bool isFemale = gen.getRandomBool();

    mRegistry.emplace<SimCharacterComponent>(newPerson, ++mUIDGenerator);
    mRegistry.emplace<SimCharacterGenderComponent>(newPerson, isFemale);
    SimPositionComponent& posCmp = mRegistry.emplace<SimPositionComponent>(newPerson, worldTilePosition, mWorld.getChunkIDAtWorldPos(worldTilePosition));
    mRegistry.emplace<SimBrainComponent>(newPerson);
    mRegistry.emplace<SimNeedsComponent>(newPerson);
    mRegistry.emplace<AttributesComponent>(newPerson).init(
        DEFAULT_HEALTH,
        DEFAULT_STAMINA,
        DEFAULT_BLOOD,
        DEFAULT_MOVE_SPEED
    );
    mRegistry.emplace<SimEntityTypeComponent>(newPerson).type = SimEntityType::Person;

    SimCharacterNameComponent& nameCmp = mRegistry.emplace<SimCharacterNameComponent>(newPerson);
    nameCmp.firstName = NameManager::getRandomFirstName(gen, isFemale);
    nameCmp.lastName = NameManager::getRandomLastName(gen);

    mEntitiesInChunks[posCmp.chunk].push_back(newPerson);

    dispatchEntityCreated(SimECSEvent{ newPerson, SimEntityType::Person });

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

ChunkEntityFullActivateDataList SimECS::simThreadOnActivateChunk(ChunkID chunkId) {
    ASSERT_SIM_THREAD();

    ChunkEntityFullActivateDataList rv;
    EntityVector& list = mEntitiesInChunks[chunkId];
    rv.resize(list.size());

    for (size_t i = 0; i < list.size(); ++i) {
        rv[i] = onFullActivateEntity(list[i]);
    }

    // Free memory
    list.clear();
    if (list.capacity() > ENTITY_LIST_DEALLOCATE_COUNT) {
        list.shrink_to_fit();
        list.reserve(ENTITY_LIST_RESERVE_COUNT);
    }

    return rv;
}

void SimECS::simThreadOnFullDeactivateEntities(ChunkID chunkId, const ChunkEntityFullDeactivateDataList& deactivateEntities) {
    ASSERT_SIM_THREAD();
    EntityVector& list = mEntitiesInChunks[chunkId];
    list.reserve(list.size() + deactivateEntities.size());
    for (const EntityFullDeactivateData& dd : deactivateEntities) {
        SimPositionComponent& posCmp = mRegistry.emplace<SimPositionComponent>(dd.simEntity);
        posCmp.position = dd.simPosition;
        posCmp.chunk = chunkId;
        list.emplace_back(dd.simEntity);

        // Remove binding
        auto&& it = mFullEntityBindings.find(dd.simEntity);
        mFullEntityBindings.erase(it);
        mRegistry.remove<FullEntityBindingComponent>(dd.simEntity);
    }
}

void SimECS::onEntityEnterNewChunk(entt::entity entity, ChunkID prevChunk, ChunkID newChunk) {
    PROFILE_FUNCTION();

    EntityVector& prevEntityList = mEntitiesInChunks[prevChunk];
    bool found = false;
    for (size_t i = 0; i < prevEntityList.size(); ++i) {
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
    }
    else {
        mFullActivatedEntitiesThisFrame[newChunk].emplace_back(onFullActivateEntity(entity));
    }
}

void SimECS::debugRender(f32v3 cameraPos) const {
    if (sDebugOptions.mShowSettlementDebug) {
        std::lock_guard lock(mDebugRenderMutex);
        mDebugCameraPos = cameraPos;
    }
    else {
        std::lock_guard lock(mDebugRenderMutex);
        mDebugCameraPos.x = FLT_MAX;
    }
}

EntityFullActivateData SimECS::onFullActivateEntity(entt::entity entity) {
    EntityFullActivateData rv;
    rv.entityType = mRegistry.get<SimEntityTypeComponent>(entity).type;
    rv.simPosition = mRegistry.get<SimPositionComponent>(entity).position;
    rv.simEntity = entity;
    // We erase our sim position while fully activated
    mRegistry.remove<SimPositionComponent>(entity);

    // Create binding
    SimFullEntityBinding& binding = mFullEntityBindings[entity];
    rv.binding = &binding;
    binding.simEntity = entity;
    mRegistry.emplace<FullEntityBindingComponent>(entity).binding = &binding;

    return rv;
}

void SimECS::onEntityFullActivationFailed(ChunkID chunkId, ChunkEntityFullActivateDataList&& activateData) {
    ASSERT_SIM_THREAD();
    if (mHostSimContext.isChunkSimulating(chunkId)) {
        ChunkEntityFullDeactivateDataList list;
        list.resize(activateData.size());
        for (size_t i = 0; i < activateData.size(); ++i) {
            list[i].simEntity = activateData[i].simEntity;
            list[i].simPosition = activateData[i].simPosition;
        }
        simThreadOnFullDeactivateEntities(chunkId, list);
    }
    else {
        // Fail again! either due to delay or due to chunk immediately reactivating, just keep ping ponging back till it owrks
        ChunkEntityFullActivateDataList& list = mFullActivatedEntitiesThisFrame[chunkId];
        if (list.empty()) {
            list.swap(activateData);
        }
        else {
            // Append
            list.reserve(list.size() + activateData.size());
            list.insert(list.end(), activateData.begin(), activateData.end());
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
        avgMoveSpeed += mRegistry.get<AttributesComponent>(members[i]).getCurrentAttribute(AttributeType::MoveSpeed);
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
