#pragma once

// Lightweight, fast access for simulation
struct SettlementSimComponent {

};

struct SettlementAdjacencyData {
    entt::entity settlementEntity = entt::null;
    f32 approxMovementCost = 0.0f; // Updated periodically
};

// Heavyweight, less accesses needed
struct SettlementDetailsComponent {
    std::vector<ChunkID> ownedChunks;
    std::vector<SettlementAdjacencyData> neighborSettlements;
};

struct FactionOwnershipComponent {
    FactionID factionId;
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