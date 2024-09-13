#include "stdafx.h"
#include "SimSettlementSystem.h"

#include "ecs/component/SimEntityTypeComponent.h"

#include "world/simulation/host/component/CharacterGroupComponents.h"
#include "world/simulation/host/component/SimCharacterComponents.h"
#include "world/simulation/host/component/SimSettlementComponents.h"
#include "world/simulation/host/settlement/SettlementPlanner.h"
#include "world/simulation/host/SimECS.h"
#include "world/IChunkGrid.h"

#include "world/World.h"
#include "world/simulation/host/HostSimContext.h"
#include "world/ownership/OwnershipGrid.h"

#include "text/NameManager.h"

constexpr TimestampMs UPDATE_INTERVAL = 5000;

SimSettlementSystem::SimSettlementSystem(HostSimContext& simContext, SimECS& ecs, entt::registry& registry) :
    mSimContext(simContext), mRegistry(registry), mECS(ecs), mCharacterInterface(*this) {

    mPlanner = std::make_unique<SettlementPlanner>(mSimContext.getWorld(), mECS, registry);
}

SimSettlementSystem::~SimSettlementSystem() = default;

void SimSettlementSystem::tick(TimestampMs currentTime, TimestampMs deltaTimeMs) {
    mCurrentTime = currentTime;
    mDeltaTimeMs = deltaTimeMs;
    mDeltaTimeSec = deltaTimeMs / MS_PER_SECOND;

    if (mCurrentTime >= mNextUpdateTime) {
       // LOG_DEBUG("Updating settlements {}", mCurrentTime);
        auto view = mRegistry.view<SettlementSimComponent, SettlementPlannerComponent>();
        for (entt::entity entity : view) {
            mPlanner->updatePlanner(entity, currentTime, deltaTimeMs);
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

    if (ownershipGrid.isChunkOwnedByAnySettlement(rootChunk)) {
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

    // This will add a ChunkOwnershipComponent
    // TODO: What if already owned?
    ownershipGrid.setChunkOwner(rootChunk, settlementEntity);

    // Add components
    SettlementDetailsComponent& detailsCmp = mRegistry.emplace<SettlementDetailsComponent>(settlementEntity);

    SettlementSimComponent& simCmp = mRegistry.emplace<SettlementSimComponent>(settlementEntity);
    simCmp.uid = mUIDGen++;
    simCmp.rootChunkId = rootChunk;
    simCmp.tier = SettlementTier::Hamlet;

    mRegistry.emplace<SettlementDistrictsComponent>(settlementEntity);
    mRegistry.emplace<SettlementPlannerComponent>(settlementEntity);
    mRegistry.emplace<SettlementJobBoardsComponent>(settlementEntity); 
    mRegistry.emplace<SettlementWorkOrdersComponent>(settlementEntity);
    mRegistry.emplace<SettlementQuartermasterComponent>(settlementEntity);
    mRegistry.emplace<SettlementLayoutComponent>(settlementEntity);
    mRegistry.emplace<SimEntityTypeComponent>(settlementEntity).type = SimEntityType::Settlement;
    mRegistry.emplace<SettlementHarvestableTrackerComponent>(settlementEntity);
    // TODO Adjacency

    const i32v2 defaultHomePoint = world.getChunkGrid().getChunk(rootChunk).getWorldPosCenter2D();

    // Assign people
    SettlementPeopleComponent& peopleCmp = mRegistry.emplace<SettlementPeopleComponent>(settlementEntity);
    peopleCmp.leader = leader;
    peopleCmp.people = people;
    for (auto& person : people) {
        DualResidentComponent& residentCmp = mRegistry.get_or_emplace<DualResidentComponent>(person);
        residentCmp.simSettlementEntity = settlementEntity;
        residentCmp.homePoint = defaultHomePoint;
    }
    // TODO: Homeless families...
    peopleCmp.homelessCount = peopleCmp.people.size();

    mECS.dispatchEntityCreated(SimECSEvent{ settlementEntity, SimEntityType::Settlement });

    // Initialize planner state (TODO: Event listener?)
    mPlanner->onSettlementCreated(settlementEntity, mCurrentTime);

    return settlementEntity;
}
