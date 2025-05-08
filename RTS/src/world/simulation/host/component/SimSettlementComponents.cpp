#include "stdafx.h"
#include "SimSettlementComponents.h"

#include "world/chunk/SimChunkGrid.h"
#include "tile/SimTileReservation.h"

// TODO: Move to SimSettlementSystem?
std::unique_ptr<SimChunkTileReservation> SettlementHarvestableTrackerComponent::tryReserveNearestHarvestable(
    TileHarvestable harvestableType, TileCoord searchCenter, SimChunkGrid& simGrid
) {
    bool didRetry = false;
    do {
        SortedIntCoordDistanceSqMap& harvestables = getLocationsForHarvestable(harvestableType);
        auto&& it = harvestables.begin();
        while (it != harvestables.end()) {
            TileCoord pos(it->second);

            SimChunkTileReservationHandle rv = simGrid.tryReserveHarvestableAtTilePos(pos, harvestableType);
            it = harvestables.erase(it);
            if (rv) {
                return rv;
            }
        }
        // Only retry once
        if (didRetry) {
            break;
        } else {
            constexpr i32 MAX_COUNT = 64;
            harvestables = simGrid.getClosestUnreservedHarvestablesToPoint(searchCenter, harvestableType, currentSearchRadiusTiles, MAX_COUNT);
            didRetry = true;
        }
    } while (true);
    return nullptr;
}
