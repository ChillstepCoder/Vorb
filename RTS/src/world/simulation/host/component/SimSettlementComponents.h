#pragma once

// TODO: MOVE
#include "item/ItemStack.h"

// TODO: SettlementConst?
#include "city/CityConst.h"
#include "world/settlement/SettlementLayoutManager.h"
#include "tile/TileHarvestable.h"

class SimChunkGrid;
class SimChunkTileReservation;

enum class JobType {
    
};

struct JobOffer {
    entt::entity jobGiver;
    std::vector<ItemStack> rewards;
};

enum class SettlementTier : ui8 {
    Hamlet,
    Village,
    Town,
    City,
    Metropolis,
    COUNT
};

// Lightweight, fast access for simulation
struct SettlementSimComponent {
    ChunkID rootChunkId;
    SettlementUID uid;
    SettlementTier tier = SettlementTier::Hamlet;

    TileCoord getCenterPos(i32 worldWidthChunks) {
        return TileCoord((rootChunkId % worldWidthChunks) * CHUNK_WIDTH, (rootChunkId / worldWidthChunks) * CHUNK_WIDTH);
    }
};

struct SettlementAdjacencyData {
    entt::entity settlementEntity = entt::null;
    f32 approxMovementCost = 0.0f; // Updated periodically
};

struct SettlementDistrict {
    ChunkID chunk;
    DistrictType type;
    std::vector<BuildingID> structures;
};

struct SettlementDistrictsComponent {
    std::vector<SettlementDistrict> districts;
};

struct SettlementRoadGraphComponent {
    // TODO?
};

// Handles logic for growing the settlement
struct SettlementPlannerComponent {
    std::vector<FamilyID> familiesPendingHomes;
    std::vector<entt::entity> singleCharactersPendingHomes;
};

struct ChunkOwnershipComponent {
    std::vector<ChunkID> ownedChunks;
};

// Heavyweight, less accesses needed
struct SettlementDetailsComponent {
    std::vector<SettlementAdjacencyData> neighborSettlements;
};

struct SettlementJobBoardData {
    i32v2 tilePos;
    std::vector<JobOffer> jobOffers;
};

struct SettlementJobBoardsComponent {
    std::vector<SettlementJobBoardData> jobBoards;
};

struct SettlementPeopleComponent {
    entt::entity leader = entt::null;
    std::vector<entt::entity> people;
    ui32 homelessCount = 0;
};

struct SettlementQuartermasterComponent {
    entt::entity quartermasterCharacter = entt::null;
    std::vector<BuildingID> storageStructures;
    UnorderedFlatMap<ItemID, ui32v2> itemCountsVsDesired;
};

struct SettlementWorkOrdersComponent {

};

// Handles plots, roads, structures
struct SettlementLayoutComponent {
    i32v2 rootPos;
    SettlementLayoutManager manager;
};

struct SettlementHarvestableTrackerComponent {
    friend class SimSettlementSystem;
    SettlementHarvestableTrackerComponent() {
        harvestableLocations = std::make_unique<SortedIntCoordDistanceSqMap[]>(e_count(TileHarvestable));
    }

    SortedIntCoordDistanceSqMap& getLocationsForHarvestable(TileHarvestable harvestable) {
        assert(harvestable != TileHarvestable::None);
        ASSERT_SIM_THREAD();
        return harvestableLocations[e_cast(harvestable)];
    }

    // Find and reserve
    std::unique_ptr<SimChunkTileReservation> tryReserveNearestHarvestable(
        TileHarvestable harvestableType, TileCoord searchCenter, SimChunkGrid& simGrid
    );

public:
    i32 currentSearchRadiusTiles = CHUNK_WIDTH * 16;
private:
    std::unique_ptr<SortedIntCoordDistanceSqMap[]> harvestableLocations;
};

//std::vector<Chunk*> mChunks;
//std::vector<std::unique_ptr<Building>> mBuildings;
//std::vector<std::unique_ptr<CityRoad>> mRoads;
//std::vector<entt::entity> mBusinesses;
//
//std::unique_ptr<CityResidentManager> mCityResidentManager;
//std::unique_ptr<CityBusinessManager> mCityBusinessManager;
//std::unique_ptr<CityPlanner> mCityPlanner;
//std::unique_ptr<CityPlotter> mCityPlotter;
//std::unique_ptr<CityBuilder> mCityBuilder;
//std::unique_ptr<CityQuartermaster> mCityQuartermaster;