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
