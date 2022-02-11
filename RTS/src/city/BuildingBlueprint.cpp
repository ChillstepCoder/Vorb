#include "stdafx.h"
#include "BuildingBlueprint.h"

#include "world/TileRepository.h"

BuildingBlueprint::BuildingBlueprint(
    const BuildingDef& desc,
    float sizeAlpha,
    Cartesian entrySide,
    ui32v2 dims,
    ui32v2 bottomLeftWorldPos,
    entt::entity ownerEntity,
    BuildingBlueprintFlags flags
) :
    desc(desc), sizeAlpha(sizeAlpha), entrySide(entrySide), aabb(bottomLeftWorldPos.x, bottomLeftWorldPos.y, dims.x, dims.y), mOwnerEntity(ownerEntity), flags(flags) {

    // TODO: Different per building
    tileIDs[enum_cast(BlueprintTileType::NONE)] = 0;
    tileIDs[enum_cast(BlueprintTileType::FLOOR_1)] = TileRepository::getTile("bricks1");
    tileIDs[enum_cast(BlueprintTileType::DOOR)] = TileRepository::getTile("door");
    tileIDs[enum_cast(BlueprintTileType::WALL)] = TileRepository::getTile("wood_wall_gothic");
    static_assert(enum_cast(BlueprintTileType::TYPES) == 4);

}

