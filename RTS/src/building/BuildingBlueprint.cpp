#include "stdafx.h"
#include "BuildingBlueprint.h"

BitArray BuildingBlueprint::computeSolidTilesFirstFloor() const {
    const TileCoord dimsTiles(dimsDTile);
    const i32 floorStride = dimsTiles.x * dimsTiles.y;
    BitArray solidTilesFirstFloor = BitArray(floorStride);
    // TODO: Optimize by sorting tileTargets in generation so we stop iterating after done with
    // first floor
    for (ui32 i = 0; i < tileTargetCount; ++i) {
        const TileIndex index = tileTargets[i].tileIndex;
        if (index < floorStride) {
            solidTilesFirstFloor.setBit(index);
        }
    }
    return solidTilesFirstFloor;
}

void BuildingBlueprint::onEndReservation(ui32 reservationId) {
    auto&& it = itemReservationHandles.find(reservationId);
    assert(it != itemReservationHandles.end());
    SimpleItemReservationTargetHandle& handle = *it->second;
    std::span<const ItemID> desiredItems = handle.getDesiredItems();
    std::span<const i32> remainingQuantities = handle.getRemainingQuantities();

    // If we have any items that were not fully filled, decrement the count from
    // the tracked promised count so other workers can then try to promise it
    for (int i = 0; i < desiredItems.size(); ++i) {
        if (remainingQuantities[i] > 0) {
            for (int j = 0; j < itemCompositionCount; ++j) {
                FillableSimpleItemStack& stack = itemComposition[j];
                stack.promisedQuantity -= remainingQuantities[i];
                totalItemsUnpromised += remainingQuantities[i];
            }
        }
    }

    itemReservationHandles.erase(it);
}
