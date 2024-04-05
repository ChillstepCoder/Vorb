#include "stdafx.h"
#include "BuildingBlueprint.h"

BitArray BuildingBlueprint::computeSolidTilesFirstFloor() const {
    const i32v2 dimsTiles = dimsDTile * DTILE_WIDTH;
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