#pragma once

#include "world/settlement/SettlementPlot.h"

class RandomGenerator;
class World;
class VisualLog;


// Based on road proximity, we generate in a direction
enum class PlotSeedDir : ui8 {
    SouthWest,
    SouthEast,
    NorthWest,
    NorthEast,
    NONE
};

struct PlotSeed {
    auto operator<=>(const PlotSeed&) const = default;
    DTileCoord pos;
    PlotSeedDir dir = PlotSeedDir::NONE;
};
namespace std {
    template<>
    struct hash<PlotSeed> {
        size_t operator()(const PlotSeed& p) const {
            size_t seed = 0;
            boost::hash_combine(seed, p.pos.x);
            boost::hash_combine(seed, p.pos.y);
            boost::hash_combine(seed, p.dir);
            return seed;
        }
    };
}

class SettlementPlotManager {
public:
    SettlementPlotManager(World& world, RandomGenerator& randomGenerator);
    // Does not change ownership grid, must be done by caller
    void addPlotSeed(DTileCoord pos, SettlementZone zone, PlotSeedDir dir);
    // Does not change ownership grid, must be done by caller
    void removePlotSeed(DTileCoord pos);
    SettlementZone getPlotSeedZone(DTileCoord pos) const;
    PlotSeed getPlotSeed(DTileCoord pos) const;

    SettlementPlot& getPlot(SettlementPlotID id) { return mPlots[id]; }
    const SettlementPlot& getPlot(SettlementPlotID id) const { return mPlots[id]; }

    // Returns INVALID_SETTLEMENT_PLOT_ID on failure, owner must not be null
    SettlementPlotID tryClaimOrGeneratePlot(SettlementPlotRequest request, entt::entity owner, OPT VisualLog* visLog);
    // Returns INVALID_SETTLEMENT_PLOT_ID on failure, owner can be null
    SettlementPlotID tryGenerateNewPlot(SettlementPlotRequest request, entt::entity owner, OPT VisualLog* visLog);

    void debugMarkPlotFree(SettlementPlotID id);
    
    const std::vector<SettlementPlot>& getPlots() const { return mPlots; }

private:
    SettlementPlotID tryGeneratePlotAtSeedInternal(SettlementPlotRequest request, PlotSeed seed, entt::entity owner, SettlementZone plotZone, OPT VisualLog* vislog);
    SettlementPlotID allocateNewPlot(std::span<DTileCoord> coords, SettlementZone zone, i32AABB2 aabbDTile, entt::entity owner);
    bool plotSatisfiesRequest(const SettlementPlot& plot, SettlementPlotRequest request) const;

    // TODO: DISTRICTING
    // TODO: Handle deleting plots!
    std::vector<SettlementPlot> mPlots;
    std::vector<SettlementPlotID> mFreePlots;
    //std::map<SettlementZone, std::vector<SettlementPlotID>> mFreePlots; // Use?
    std::map<SettlementZone, std::vector<PlotSeed>> mPlotSeeds;
    std::map<DTileCoord, SettlementZone> mPlotSeedToZone;
    World& mWorld;
    RandomGenerator& mRandomGenerator;
};
