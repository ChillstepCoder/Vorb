#pragma once

#include "math/Random.h"
#include "world/settlement/SettlementZone.h"

class World;
class SettlementRoadNetwork;
class SettlementPlotManager;
class SimECS;
class VisualLog;


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
    SettlementSector(DTileCoord center, f32 desiredRadius, SettlementSectorID id, SettlementZone zone) :
        center(center), desiredRadius(desiredRadius), id(id), zone(zone) {}

    std::vector<std::pair<ui32 /*sectionId*/, RoadSegmentID>> neighborSections;
    DTileCoord center;
    f32 desiredRadius;
    SettlementSectorID id;
    SettlementZone zone;
};

// TODO: Inside SettlementLayoutComponent
class SettlementLayoutManager {
public:
    SettlementLayoutManager();
    ~SettlementLayoutManager();
    // move must be defined in cpp due to std::unique_ptrs
    SettlementLayoutManager(SettlementLayoutManager&& o);
    SettlementLayoutManager& operator=(SettlementLayoutManager&& o);

    VORB_NON_COPYABLE(SettlementLayoutManager);

    bool tryInitAtWorldPos(World& world, entt::entity settlement, DTileCoord dTilePos);
    bool tryAddNewRandomSector();
    bool tryAddSector(DTileCoord center, f32 desiredRadius, SettlementZone zone);

    void debugDraw() const;

private:
    std::pair<SettlementZone, f32> getDesiredZoneAndRadiusAtCoord(DTileCoord coord);
    void debugInitSettlementPartiallyMade();

    std::vector<SettlementSector> mSectors;
    std::vector<ui32> mOpenSectors; // Sectors that have at least one road edge to infinity
    std::unique_ptr<SettlementPlotManager> mPlotManager;
    std::unique_ptr<SettlementRoadNetwork> mRoadNetwork;
    entt::entity mSettlementEntity = entt::null;
    World* mWorld = nullptr;
    SimECS* mSimEcs = nullptr;
    DTileCoord mRootPos;
    VisualLog* mCurrentVisLog = nullptr;
    RandomGenerator mRandomGenerator;
    f32v2 mSettlementOrientation; // Settlement growth follows a directional pattern
};
