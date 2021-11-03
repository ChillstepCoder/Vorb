#include "stdafx.h"
#include "BuildingBlueprint.h"

#include "world/TileRepository.h"

BuildingBlueprint::BuildingBlueprint(
    const BuildingDescription& desc,
    float sizeAlpha,
    Cartesian entrySide,
    ui16v2 dims,
    ui32v2 bottomLeftWorldPos
) :
    desc(desc), sizeAlpha(sizeAlpha), entrySide(entrySide), dims(dims), bottomLeftWorldPos(bottomLeftWorldPos) {

    // TODO: Different per building
    tileIDs[enum_cast(BlueprintTileType::NONE)] = 0;
    tileIDs[enum_cast(BlueprintTileType::FLOOR_1)] = TileRepository::getTile("bricks1");
    tileIDs[enum_cast(BlueprintTileType::DOOR)] = TileRepository::getTile("door");
    tileIDs[enum_cast(BlueprintTileType::WALL)] = TileRepository::getTile("wood_wall_gothic");
    static_assert(enum_cast(BlueprintTileType::TYPES) == 4);

}

