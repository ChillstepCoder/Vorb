#pragma once

#include "util/BitArray.h"
#include "world/settlement/SettlementZone.h"

class RandomGenerator;
class World;

enum class PlotFlags : ui8 {
    Owned,
    Reserved,
    HasBlueprint,
    HasFinishedStructure
};

struct SettlementPlot {
    BitArray ownedDTiles;
    i32AABB2 aabbDTile;
    RoadSegmentID connectedRoad;
    BitFlags<PlotFlags> flags;
    SettlementZone zone;
    //ui8 padding[2];
};

struct SettlementPlotRequest {
    i32 minimumWidth = 5;
    i32 maximumWidth = 10;
    i32 minimumSize = SQ(5);
    i32 maximumSize = SQ(10);
    SettlementZone zone;
    // DTileCoord desiredProximity (for generating close to forests?)
};

class SettlementPlotManager {
public:
    SettlementPlotManager(World& world, RandomGenerator& randomGenerator);
    // Does not change ownership grid, must be done by caller
    void addPlotSeed(DTileCoord pos, SettlementZone zone);
    // Does not change ownership grid, must be done by caller
    void removePlotSeed(DTileCoord pos);
    SettlementZone getPlotSeedZone(DTileCoord pos) const;

    const SettlementPlot& getPlot(SettlementPlotID id) const { return mPlots[id]; }
    // Returns INVALID_SETTLEMENT_PLOT_ID on failure
    SettlementPlotID tryGenerateNewPlot(SettlementPlotRequest request, entt::entity owner);
    
    const std::vector<SettlementPlot>& getPlots() const { return mPlots; }

private:
    SettlementPlotID tryGeneratePlotAtSeedInternal(SettlementPlotRequest request, DTileCoord seed, entt::entity owner);
    SettlementPlotID allocateNewPlot(std::span<DTileCoord> coords, SettlementZone zone, i32AABB2 aabbDTile, entt::entity owner);
    // TODO: DISTRICTING
    std::vector<SettlementPlot> mPlots;
    //std::map<SettlementZone, std::vector<SettlementPlotID>> mFreePlots; // Use?
    std::map<SettlementZone, std::vector<DTileCoord>> mPlotSeeds;
    std::map<DTileCoord, SettlementZone> mPlotSeedToZone;
    World& mWorld;
    RandomGenerator& mRandomGenerator;
};
