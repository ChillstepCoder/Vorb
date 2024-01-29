#pragma once

// TODO: MOVE
#include "item/ItemStack.h"

// TODO: SettlementConst?
#include "city/CityConst.h"

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
    SettlementTier tier = SettlementTier::Hamlet;
};

struct SettlementAdjacencyData {
    entt::entity settlementEntity = entt::null;
    f32 approxMovementCost = 0.0f; // Updated periodically
};

struct SettlementDistrict {
    ChunkID chunk;
    DistrictType type;
    std::vector<StructureID> structures;
};

struct SettlementDistrictsComponent {
    std::vector<SettlementDistrict> districts;
};

struct SettlementRoadGraphComponent {
    // TODO?
};

// Handles logic for growing the settlement
struct SettlementPlannerComponent {
};

// Heavyweight, less accesses needed
struct SettlementDetailsComponent {
    std::vector<ChunkID> ownedChunks;
    std::vector<SettlementAdjacencyData> neighborSettlements;
};

struct SettlementJobBoardData {
    i32v2 tilePos;
    std::vector<JobOffer> jobOffers;
};

struct SettlementJobBoardsComponent {
    std::vector<SettlementJobBoardData> jobBoards;
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