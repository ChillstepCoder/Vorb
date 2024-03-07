#include "stdafx.h"
#include "SimSettlementSystem.h"

#include "world/simulation/host/component/SimComponents.h"
#include "world/simulation/host/component/SettlementComponents.h"
#include "world/simulation/host/settlement/SettlementPlanner.h"
#include "world/simulation/host/SimECS.h"

#include "world/World.h"
#include "world/simulation/host/HostSimContext.h"
#include "world/ownership/OwnershipGrid.h"

#include "text/NameManager.h"

constexpr TimestampMs UPDATE_INTERVAL = 5000;

SimSettlementSystem::SimSettlementSystem(HostSimContext& simContext, SimECS& ecs, entt::registry& registry) :
    mSimContext(simContext), mRegistry(registry), mECS(ecs) {

    mPlanner = std::make_unique<SettlementPlanner>(mSimContext.getWorld(), registry);
}

SimSettlementSystem::~SimSettlementSystem() = default;

void SimSettlementSystem::tick(TimestampMs currentTime, TimestampMs deltaTime) {
    mCurrentTime = currentTime;
    mDeltaTime = deltaTime;

    if (mCurrentTime >= mNextUpdateTime) {
        LOG_DEBUG("Updating settlements {}", mCurrentTime);
        auto view = mRegistry.view<SettlementSimComponent, SettlementPlannerComponent>();
        for (entt::entity entity : view) {
            mPlanner->updatePlanner(entity, currentTime, deltaTime);
        }

        mNextUpdateTime = mCurrentTime + UPDATE_INTERVAL;
    }
    //RandomGenerator& gen = mSimContext.getSimRandomGenerator();
}

bool SimSettlementSystem::tryCreateSettlementFromGroup(entt::entity groupEntity) {

    CharacterGroupComponent& groupCmp = mRegistry.get<CharacterGroupComponent>(groupEntity);
    assert(groupCmp.leader != entt::null);

    SimPositionComponent& posCmp = mRegistry.get<SimPositionComponent>(groupCmp.leader);
    ChunkID rootChunk = posCmp.getChunk();

    World& world = mSimContext.getWorld();
    OwnershipGrid& ownershipGrid = world.getOwnershipGrid();

    if (ownershipGrid.isChunkOwnedBySettlement(rootChunk)) {
        LOG_CRITICAL("Ownership fail in SimSettlementSystem::tryCreateSettlementFromGroup");
        return false;
    }

    // Create entity
    entt::entity settlementEntity = createSettlementEntity(rootChunk, groupCmp.leader, groupCmp.groupMembers);

    return true;
}

entt::entity SimSettlementSystem::createSettlementEntity(ChunkID rootChunk, entt::entity leader, std::vector<entt::entity>& people) {
    World& world = mSimContext.getWorld();
    OwnershipGrid& ownershipGrid = world.getOwnershipGrid();

    entt::entity settlementEntity = mRegistry.create();

    ownershipGrid.setChunkSettlementOwner(rootChunk, settlementEntity);

    // Add components
    SettlementDetailsComponent& detailsCmp = mRegistry.emplace<SettlementDetailsComponent>(settlementEntity);
    detailsCmp.ownedChunks.emplace_back(rootChunk);

    SettlementSimComponent& simCmp = mRegistry.emplace<SettlementSimComponent>(settlementEntity);
    simCmp.uid = mUIDGen++;
    simCmp.rootChunkId = rootChunk;
    simCmp.tier = SettlementTier::Hamlet;

    mRegistry.emplace<SettlementDistrictsComponent>(settlementEntity);
    mRegistry.emplace<SettlementPlannerComponent>(settlementEntity);
    mRegistry.emplace<SettlementJobBoardsComponent>(settlementEntity); 
    mRegistry.emplace<SettlementStructuresComponent>(settlementEntity);
    mRegistry.emplace<SettlementWorkOrdersComponent>(settlementEntity);
    mRegistry.emplace<SettlementQuartermasterComponent>(settlementEntity);
    mRegistry.emplace<SettlementLayoutComponent>(settlementEntity);
    // TODO Adjacency

    // Assign people
    SettlementPeopleComponent& peopleCmp = mRegistry.emplace<SettlementPeopleComponent>(settlementEntity);
    peopleCmp.leader = leader;
    peopleCmp.people = people;
    for (auto& person : people) {
        SimResidentComponent& residentCmp = mRegistry.get_or_emplace<SimResidentComponent>(person);
        residentCmp.settlementEntity = settlementEntity;
    }
    // TODO: Homeless families...
    peopleCmp.homelessCount = peopleCmp.people.size();

    mECS.dispatchEntityCreated(SimECSEvent{ settlementEntity, SimEntityType::Settlement });

    // Initialize planner state (TODO: Event listener?)
    mPlanner->onSettlementCreated(settlementEntity, mCurrentTime);

    return settlementEntity;
}
