#include "stdafx.h"
#include "SettlementPlotManager.h"

#include "math/Random.h"

SettlementPlotManager::SettlementPlotManager(RandomGenerator& randomGenerator) : mRandomGenerator(randomGenerator) {

}

void SettlementPlotManager::addPlotSeed(DTileCoord pos) {
    mPlotSeeds.emplace_back(pos);
}

void SettlementPlotManager::removePlotSeed(DTileCoord pos) {
    for (size_t i = 0; i < mPlotSeeds.size(); ++i) {
        if (mPlotSeeds[i] == pos) {
            mPlotSeeds[i] = mPlotSeeds.back();
            mPlotSeeds.pop_back();
            break;
        }
    }
}
