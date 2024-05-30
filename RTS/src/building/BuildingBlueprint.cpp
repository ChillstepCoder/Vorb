#include "stdafx.h"
#include "BuildingBlueprint.h"

BitArray BuildingBlueprint::computeSolidTilesFirstFloor() const {
    const TileCoord dimsTiles(dimsDTile);
    const i32 floorStride = dimsTiles.x * dimsTiles.y;
    BitArray solidTilesFirstFloor = BitArray(floorStride);
    for (ui32 i = 0; i < tileTargetCount; ++i) {
        const TileIndex index = tileTargets[i].tileIndex;
        if (index < floorStride) {
            solidTilesFirstFloor.setBit(index);
        }
        else {
            break;
        }
    }
    for (ui32 i = 0; i < stairTargetCount; ++i) {
        const TileIndex index = stairTargets[i].piece.pos;
        if (index < floorStride) {
            solidTilesFirstFloor.setBit(index);
        }
        else {
            break;
        }
    }
    return solidTilesFirstFloor;
}

FillableRecipe& BuildingBlueprint::getRecipeForTargetData(BuildContextTargetData data) const {
    switch (data.type) {
        case BuildContextTargetData::Type::Tile:
            return tileTargets[data.targetIndex].fillableRecipe;
        case BuildContextTargetData::Type::Wall:
            return wallTargets[data.targetIndex].fillableRecipe;
        case BuildContextTargetData::Type::Stairs:
            return stairTargets[data.targetIndex].fillableRecipe;
    }
    static_assert(e_count(BuildContextTargetData::Type) == 3);
    panic("Invalid target type in BuildingBlueprint::getRecipeForTargetData");
}

void BuildingBlueprint::onEndItemReservation(ui32 reservationId) {
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
