#pragma once

#include "SettlementRoadNetwork.h"

class SimECS;

typedef ui32 SettlementSectorID;
constexpr SettlementSectorID INVALID_SECTOR_ID = std::numeric_limits<SettlementSectorID>::max();

// Minimum span for quick tracing
//struct RoadSegmentTraceInfo {
//    DTileCoord verts[2];
//    bool infiniteEdges[2] = {};
//    RoadSegmentType type;
//};

struct SettlementSector {
    SettlementSector() = default;
    SettlementSector(DTileCoord center, f32 desiredRadius, SettlementSectorID id) : center(center), desiredRadius(desiredRadius), id(id) {}

    std::vector<std::pair<ui32 /*sectionId*/, RoadSegmentID>> neighborSections;
    DTileCoord center;
    f32 desiredRadius;
    SettlementSectorID id;
};

// TODO: Inside SettlementLayoutComponent
class SettlementLayoutManager {
public:
    bool tryInitAtWorldPos(World& world, entt::entity settlement, DTileCoord dTilePos);
    bool tryAddNewRandomSector();
    bool tryAddSector(DTileCoord center, f32 desiredRadius);

    void debugDraw() const;

    std::vector<SettlementSector> mSectors;
    std::vector<ui32> mOpenSectors; // Sectors that have at least one road edge to infinity
    SettlementRoadNetwork mRoadNetwork;
    entt::entity mSettlementEntity = entt::null;
    World* mWorld = nullptr;
    SimECS* mSimEcs = nullptr;
    DTileCoord mRootPos;
    VisualLog* mCurrentVisLog = nullptr;
};