#include "stdafx.h"
#include "SettlementPlotManager.h"

#include "math/Random.h"
#include "world/World.h"

#include "world/ownership/OwnershipGrid.h"
#include "world/IHeightmapGrid.h"

static std::unordered_set<i32> sClosedSeedSet;

SettlementPlotManager::SettlementPlotManager(World& world, RandomGenerator& randomGenerator) : mWorld(world), mRandomGenerator(randomGenerator) {

}

void SettlementPlotManager::addPlotSeed(DTileCoord pos, SettlementZone zone, PlotSeedDir dir) {
    ASSERT_SIM_THREAD();
    assert(!mPlotSeedToZone.contains(pos));
    mPlotSeedToZone.emplace(pos, zone);
    mPlotSeeds[zone].emplace_back(PlotSeed{ pos, dir });
}

void SettlementPlotManager::removePlotSeed(DTileCoord pos) {
    ASSERT_SIM_THREAD();
    auto it = mPlotSeedToZone.find(pos);
    if (it == mPlotSeedToZone.end()) [[unlikely]] {
        return;
    }
    auto it2 = mPlotSeeds.find(it->second);
    assert(it2 != mPlotSeeds.end());
    std::vector<PlotSeed>& seeds = it2->second;
    for (size_t i = 0; i < seeds.size(); ++i) {
        if (seeds[i].pos == pos) {
            seeds[i] = seeds.back();
            seeds.pop_back();
            if (seeds.empty()) {
                mPlotSeeds.erase(it2);
            }
            break;
        }
    }
    mPlotSeedToZone.erase(it);
}

SettlementZone SettlementPlotManager::getPlotSeedZone(DTileCoord pos) const {
    ASSERT_SIM_THREAD();
    auto it = mPlotSeedToZone.find(pos);
    if (it != mPlotSeedToZone.end()) [[likely]] {
        return it->second;
    }
    return SettlementZone::COUNT;
}

PlotSeed SettlementPlotManager::getPlotSeed(DTileCoord pos) const {
    auto it = mPlotSeedToZone.find(pos);
    if (it != mPlotSeedToZone.end()) [[likely]] {
        auto it2 = mPlotSeeds.find(it->second);
        assert(it2 != mPlotSeeds.end());
        for (const PlotSeed& s : it2->second) {
            if (s.pos == pos) {
                return s;
            }
        }
    }
    return PlotSeed();
}

SettlementPlotID SettlementPlotManager::tryGenerateNewPlot(SettlementPlotRequest request, entt::entity owner) {
    ASSERT_SIM_THREAD();

    auto it = mPlotSeeds.find(request.zone);
    if (it == mPlotSeeds.end()) {
        return INVALID_SETTLEMENT_PLOT_ID;
    }
    const i32 worldWidthDTiles = mWorld.getWidthDTiles();
    //sClosedSeedSet.clear();
    //sClosedSeedSet.reserve(256);

    std::vector<PlotSeed>& seeds = it->second;
    for (PlotSeed c : seeds) {
        const ui32 hash = c.pos.y * worldWidthDTiles + c.pos.x;
        /*if (sClosedSeedSet.contains(hash)) {
            continue;
        }*/
        //sClosedSeedSet.insert(hash);
        SettlementPlotID plotId = tryGeneratePlotAtSeedInternal(request, c, owner);
        if (plotId != INVALID_SETTLEMENT_PLOT_ID) {
            return plotId;
        }
    }

    // TODO
    return INVALID_SETTLEMENT_PLOT_ID;
}

SettlementPlotID SettlementPlotManager::tryGeneratePlotAtSeedInternal(SettlementPlotRequest request, PlotSeed seed, entt::entity owner) {
    OwnershipGrid& ownerGrid = mWorld.getOwnershipGrid();
    IHeightmapGrid& heightGrid = mWorld.getHeightmapGrid();
    const i32 worldWidthDTiles = mWorld.getWidthDTiles();
    // Simple scan line search, possibly bad
    // We first search to the right. If fail, retry and search to the left.
    // TODO: Cache this memory?
    std::vector<DTileCoord> validPoints;
    validPoints.reserve(request.maximumSize + 1);

    i32 minX = INT32_MAX;
    i32 maxX = INT32_MIN;
    i32 minY = INT32_MAX;
    i32 maxY = INT32_MIN;

    bool startValid = false;
    DTileCoord start = seed.pos;
    for (i32 y = 0; y < request.maximumWidth; ++y) {
        for (i32 x = 0; x < request.maximumWidth; ++x) {
            DTileCoord newCoord(start.x + x, start.y + y);
            if (newCoord.x >= worldWidthDTiles) [[unlikely]] {
                if (!startValid) continue;
                break;
            }
            if (heightGrid.getHeightAtVert<true>(newCoord) <= -1.0f) {
                // No plots on deep water (for now)
                if (!startValid) continue;
                break;
            }
            if (const DTileOwnershipData* ownerData = ownerGrid.tryGetDTileOwnerData(newCoord)) {
                // We can only cover empty or plot seed tiles
                if (!(ownerData->ownerObjectType == DTileOwnerObjectType::None || ownerData->ownerObjectType == DTileOwnerObjectType::RoadPlotSeed)) {
                    if (!startValid) continue;
                    break;
                }
            }

            startValid = true;
            validPoints.emplace_back(newCoord);
            if (newCoord.x < minX) minX = newCoord.x;
            if (newCoord.x > maxX) maxX = newCoord.x;
            if (newCoord.y < minY) minY = newCoord.y;
            if (newCoord.y > maxY) maxY = newCoord.y;

            if (validPoints.size() == request.maximumSize) {
                return allocateNewPlot(std::span(validPoints.data(), validPoints.size()), request.zone, i32AABB2(minX, minY, maxX - minX, maxY - minY), owner);
            }
        }
        startValid = false;
    }

    return INVALID_SETTLEMENT_PLOT_ID;
}

SettlementPlotID SettlementPlotManager::allocateNewPlot(std::span<DTileCoord> coords, SettlementZone zone, i32AABB2 aabbDTile, entt::entity owner) {
    OwnershipGrid& ownerGrid = mWorld.getOwnershipGrid();
    SettlementPlotID id = mPlots.size();
    SettlementPlot& newPlot = mPlots.emplace_back();
    newPlot.zone = zone;
    newPlot.aabbDTile = aabbDTile;
    newPlot.ownedDTiles.resizeAndZero(aabbDTile.width * aabbDTile.depth);
    for (DTileCoord c : coords) {
        const i32 x = c.x - aabbDTile.pos.x;
        const i32 y = c.y - aabbDTile.pos.y;
        newPlot.ownedDTiles.setBit(y * aabbDTile.width + x);
        if (DTileOwnershipData* ownerData = ownerGrid.tryGetDTileOwnerDataForEditSimThread(c)) {
            if (ownerData->ownerObjectType == DTileOwnerObjectType::RoadPlotSeed) {
                removePlotSeed(c);
            }
            ownerGrid.setDTileDataOwner(ownerData, owner, DTileOwnerObjectType::Plot, id);
        }
        else {
            ownerGrid.setDTileOwner(c, owner, DTileOwnerObjectType::Plot, id);
        }
    }
    return id;
}
