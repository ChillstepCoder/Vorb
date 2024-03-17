#pragma once

class RandomGenerator;

enum class PlotFlags : ui8 {
    Owned,
    Reserved,
    HasBlueprint,
    HasFinishedStructure
};

struct SettlementPlot {
    i32AABB2 aabbDTileCoord;
    RoadSegmentID connectedRoad;
    BitFlags<PlotFlags> flags;
    //ui8 padding[3];
};

class SettlementPlotManager {
public:
    SettlementPlotManager(RandomGenerator& randomGenerator);
    // Does not change ownership grid, must be done by caller
    void addPlotSeed(DTileCoord pos);
    // Does not change ownership grid, must be done by caller
    void removePlotSeed(DTileCoord pos);
private:
    // TODO: DISTRICTING
    std::vector<SettlementPlot> mPlots;
    std::vector<DTileCoord> mPlotSeeds;
    RandomGenerator& mRandomGenerator;
};

