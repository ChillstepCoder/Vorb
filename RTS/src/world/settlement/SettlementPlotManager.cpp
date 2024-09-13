#include "stdafx.h"
#include "SettlementPlotManager.h"

#include "world/World.h"

#include "world/ownership/OwnershipGrid.h"
#include "world/IHeightmapGrid.h"

#include "debugging/VisualLogger.h"

#include "world/settlement/SettlementDebugHelpers.inl"

static UnorderedFlatSet<i32> sClosedSeedSet;

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
    return SettlementZone::TERM;
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

SettlementPlotID SettlementPlotManager::tryClaimOrGeneratePlot(SettlementPlotRequest request, entt::entity owner, OPT VisualLog* visLog) {
    for (SettlementPlotID plotId : mFreePlots) {
        SettlementPlot& plot = mPlots[plotId];
        if (plotSatisfiesRequest(plot, request)) {
            plot.owner = owner;
            return plotId;
        }
    }
    return tryGenerateNewPlot(request, owner, visLog);
}

SettlementPlotID SettlementPlotManager::tryGenerateNewPlot(SettlementPlotRequest request, entt::entity owner, OPT VisualLog* visLog) {
    ASSERT_SIM_THREAD();

    for (int zoneBit = BIT(0); zoneBit < (int)SettlementZone::TERM; zoneBit = zoneBit << 1) {
        // Try all allowed zones
        SettlementZone zone = (SettlementZone)zoneBit;
        if (request.allowedZones.isBitSet(zone)) {
            auto it = mPlotSeeds.find(zone);
            if (it == mPlotSeeds.end()) {
                return INVALID_SETTLEMENT_PLOT_ID;
            }
            const i32 worldWidthDTiles = mWorld.getWidthDTiles();
            //sClosedSeedSet.clear();
            //sClosedSeedSet.reserve(256);

            const std::vector<PlotSeed>& seeds = it->second;
            for (PlotSeed c : seeds) {
                const ui32 hash = c.pos.y * worldWidthDTiles + c.pos.x;
                /*if (sClosedSeedSet.contains(hash)) {
                    continue;
                }*/
                //sClosedSeedSet.insert(hash);
                SettlementPlotID plotId = tryGeneratePlotAtSeedInternal(request, c, owner, zone, visLog);
                if (plotId != INVALID_SETTLEMENT_PLOT_ID) {
                    return plotId;
                }
            }
        }
    }

    return INVALID_SETTLEMENT_PLOT_ID;
}

void SettlementPlotManager::debugMarkPlotFree(SettlementPlotID id) {
    // TODO: This is unsafe
    mFreePlots.emplace_back(id);
}

i32v2 PLOT_EXPAND_OFFSETS[4] = {
    { 0, -1}, // SOUTH
    {-1,  0}, // WEST
    { 1,  0}, // EAST
    { 0,  1}  // NORTH
};

i32v2 PLOT_ITERATE_OFFSETS[4] = {
    { 1,  0}, // SOUTH
    { 0,  1}, // WEST
    { 0,  1}, // EAST
    { 1,  0}  // NORTH
};

bool tryExpandPlotInDir(Cartesian dir, i32AABB2& aabb, std::vector<DTileCoord>& validPoints, World& world, i32 minExpandWidth, VisualLog* visLog) {

    OwnershipGrid& ownerGrid = world.getOwnershipGrid();
    IHeightmapGrid& heightGrid = world.getHeightmapGrid();
    const size_t startValidPointsSize = validPoints.size();
    const i32 worldWidthDTiles = world.getWidthDTiles();

    i32AABB2 newAABB = aabb;
    DTileCoord iterPos;
    i32 length;
    switch (dir) {
        case Cartesian::SOUTH:
            --newAABB.pos.y;
            length = newAABB.width;
            iterPos.v = newAABB.pos;
            ++newAABB.depth;
            break;
        case Cartesian::WEST:
            --newAABB.pos.x;
            length = newAABB.depth;
            iterPos.v = newAABB.pos;
            ++newAABB.width;
            break;
        case Cartesian::EAST:
            length = newAABB.depth;
            iterPos.v = newAABB.pos;
            iterPos.x += newAABB.width;
            ++newAABB.width;
            break;
        case Cartesian::NORTH:
            length = newAABB.width;
            iterPos.v = newAABB.pos;
            iterPos.y += newAABB.depth;
            ++newAABB.depth;
            break;
        default:
            assert(false);
            break;
    }

    // Check if we can expand in this direction
    const i32v2& iterateOffset = PLOT_ITERATE_OFFSETS[e_cast(dir)];
    // i32 tilesAdded = 0;
    bool isValid = false;
    i32 bestRun = 0;
    i32 currentRun = 0;
    for (int j = 0; j < length; ++j) {
        if (iterPos.x < 0 || iterPos.x >= worldWidthDTiles || iterPos.y < 0 || iterPos.y >= worldWidthDTiles) [[unlikely]] {
            isValid = false;
            break;
        }
        const f32 height = heightGrid.getHeightAtVert<true>(iterPos);
        if (height <= -1.0f) {
            // No plots on deep water (for now)
            currentRun = 0;
            if (visLog) {
                i32v2 tilepos = iterPos.toTilePos();
                visLog->addWireQuad(f32v3(tilepos.x, tilepos.y, height), f32v2(1.0f), color::Blue);
            }
            continue;
        }
        if (const DTileOwnershipData* ownerData = ownerGrid.tryGetDTileOwnerData(iterPos)) {
            // Blocked fails the whole plot attempt
            if (ownerData->ownerObjectType == DTileOwnerObjectType::ExternalRoadBlocked) {
                isValid = false;
                if (visLog) {
                    i32v2 tilepos = iterPos.toTilePos();
                    visLog->addWireQuad(f32v3(tilepos.x, tilepos.y, height), f32v2(1.0f), color::Red);
                }
                break;
            }
            // We can only cover empty or plot seed tiles but this is not a failure case
            if (!(ownerData->ownerObjectType == DTileOwnerObjectType::None || ownerData->ownerObjectType == DTileOwnerObjectType::RoadPlotSeed)) {
                currentRun = 0;

                if (visLog) {
                    i32v2 tilepos = iterPos.toTilePos();
                    visLog->addWireQuad(f32v3(tilepos.x, tilepos.y, height), f32v2(1.0f), color::Gray);
                }
                continue;
            }
        }
        isValid = true;
        if (++currentRun > bestRun) {
            bestRun = currentRun;
        }
        validPoints.emplace_back(iterPos);
        if (visLog) {
            i32v2 tilepos = iterPos.toTilePos();
            visLog->addWireQuad(f32v3(tilepos.x, tilepos.y, height), f32v2(1.0f), color4(0, 200, 0, 128));
        }
        // Step
        iterPos.v += iterateOffset;
    }
    if (!isValid || bestRun < minExpandWidth) {
        validPoints.resize(startValidPointsSize);
        return false;
    }
    aabb = newAABB;
    return true;
}

SettlementPlotID SettlementPlotManager::tryGeneratePlotAtSeedInternal(SettlementPlotRequest request, PlotSeed seed, entt::entity owner, SettlementZone plotZone, OPT VisualLog* vislog) {
    OwnershipGrid& ownerGrid = mWorld.getOwnershipGrid();
    IHeightmapGrid& heightGrid = mWorld.getHeightmapGrid();
    const i32 worldWidthDTiles = mWorld.getWidthDTiles();

    std::vector<DTileCoord> validPoints;
    validPoints.reserve(request.maximumSize + 1);

    i32AABB2 aabb(seed.pos.x, seed.pos.y, 1, 1);

    i32 validDirCount = 4;
    bool validDirs[4] = { true, true, true, true };

    // Make sure we have consecutive runs that are large enough
    int currentMaxWidth = 1;
    do {
        const i32 minExpandRunWidth = glm::min(currentMaxWidth, request.minimumWidth);
        for (i32 i = 0; i <= (i32)Cartesian::NORTH; ++i) {
            if (validDirs[i]) {
                if (!tryExpandPlotInDir((Cartesian)i, aabb, validPoints, mWorld, minExpandRunWidth, vislog)) {
                    validDirs[i] = false;
                    --validDirCount;
                }
                else if (validPoints.size() >= request.maximumSize) {
                    // We expanded beyond maximum size
                    validDirCount = 0;
                    break;
                }
                else {
                    // Enable retry expanding to our neighbor directions since we may have opened up a new valid run to push into
                    const Cartesian* neighbors = CARTESIAN_NEIGHBORS[i];
                    for (int j = 0; j < 2; ++j) {
                        if (!validDirs[e_cast(neighbors[j])]) {
                            validDirs[e_cast(neighbors[j])] = true;
                            ++validDirCount;
                        }
                    }
                }
            }
        }
        ++currentMaxWidth;
    } while (validDirCount);

    if (validPoints.size() >= request.minimumSize) {
        // TODO: Detect disjoint nodes and remove them?
        return allocateNewPlot(std::span(validPoints.data(), validPoints.size()), plotZone, aabb, owner);
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
    newPlot.owner = owner;
    newPlot.dTileCount = coords.size();
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

    if (owner == entt::null) {
        mFreePlots.emplace_back(id);
    }

    return id;
}

bool SettlementPlotManager::plotSatisfiesRequest(const SettlementPlot& plot, SettlementPlotRequest request) const {
    return (request.allowedZones.isBitSet(plot.zone)) &&
           (plot.dTileCount >= request.minimumSize) && (plot.dTileCount <= request.maximumSize) &&
           (plot.aabbDTile.dims.x >= request.minimumWidth) && (plot.aabbDTile.dims.y >= request.minimumWidth);
}
