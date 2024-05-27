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
    std::span<ItemID> desiredItems = handle.getDesiredItems();
    std::span<i32> filledQuantities = handle.getFilledQuantities();
    std::span<i32> desiredQuantities = handle.getDesiredQuantities();

    // If we have any items that were not fully filled, decrement the count from
    // the tracked fullfilled count so other workers can then try to fill it
    for (int i = 0; i < desiredItems.size(); ++i) {
        const i32 difference = desiredQuantities[i] - filledQuantities[i];
        if (difference > 0) {
            for (int j = 0; j < itemCompositionCount; ++j) {
                FillableSimpleItemStack& stack = itemComposition[j];
                if (stack.itemId = desiredItems[i]) {
                    stack.filledQuantity -= difference;
                    totalItemsUnfulfilled += difference;
                    break;
                }
            }
        }
    }

    itemReservationHandles.erase(it);
}
