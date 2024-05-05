#include "stdafx.h"
#include "SimECS.h"

#include "world/simulation/host/HostSimContext.h"
#include "world/simulation/host/component/SimComponents.h"
#include "world/simulation/host/component/SettlementComponents.h"

#include "world/simulation/host/system/SimAISystem.h"
#include "world/simulation/host/system/SimSettlementSystem.h"
#include "ecs/component/FullEntityBindingComponent.h"

#include "text/NameManager.h"

#include "math/Random.h"

#include "world/World.h"

#include "options/DebugOptions.h"

SimECS::SimECS(HostSimContext& hostSimContext) : mHostSimContext(hostSimContext), mWorld(hostSimContext.getWorld()) {
    mAISystem = std::make_unique<SimAISystem>(hostSimContext, *this, mRegistry);
    mSettlementSystem = std::make_unique<SimSettlementSystem>(hostSimContext, *this, mRegistry);
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

    debugRenderInternal();
}

entt::entity SimECS::createNewPerson(f32v2 worldTilePosition) {
    ASSERT_SIM_THREAD();

    entt::entity newPerson = mRegistry.create();
    RandomGenerator& gen = mHostSimContext.getSimRandomGenerator();

    const bool isFemale = gen.getRandomBool();

    mRegistry.emplace<SimCharacterComponent>(newPerson, ++mUIDGenerator);
    mRegistry.emplace<SimCharacterGenderComponent>(newPerson, isFemale);
    mRegistry.emplace<SimPositionComponent>(newPerson, worldTilePosition, mWorld.getChunkIDAtWorldPos(worldTilePosition));
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

    dispatchEntityDestroyed(SimECSEvent{ group, SimEntityType::Group });
    mRegistry.remove<CharacterGroupLeaderComponent>(groupCmp.leader);
    mRegistry.destroy(group);
}

ChunkEntityFullActivateDataList SimECS::simThreadOnActivateChunk(ChunkID chunkId) {
    // TODO: Handle other entities too, not just AI
    ChunkEntityFullActivateDataList list = mAISystem->simThreadOnActivateChunk(chunkId);

    // Create bindings
    for (auto& it : list) {
        SimFullEntityBinding& binding = mFullEntityBindings[it.simEntity];
        it.binding = &binding;
        mRegistry.emplace<FullEntityBindingComponent>(it.simEntity).binding = &binding;
    }

    return list;
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

    return groupEntity;
}
