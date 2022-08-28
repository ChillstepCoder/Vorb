#include "stdafx.h"
#include "BuildingBlueprint.h"

#include "resources/TileRepository.h"

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
    tileIDs[e_cast(BlueprintTileType::NONE)] = TILE_ID_NONE;
    tileIDs[e_cast(BlueprintTileType::FLOOR)] = TileRepository::getTile("bricks1");
    tileIDs[e_cast(BlueprintTileType::DOOR)] = TileRepository::getTile("door");
    tileIDs[e_cast(BlueprintTileType::WALL)] = TileRepository::getTile("wood_wall_gothic");
    tileIDs[e_cast(BlueprintTileType::STAIRS)] = TileRepository::getTile("stairs_wood");
    tileIDs[e_cast(BlueprintTileType::STAIRS_FLAT)] = TileRepository::getTile("stairs_wood_flat");
    tileIDs[e_cast(BlueprintTileType::AIR)] = TILE_ID_NONE;

    static_assert(e_cast(BlueprintTileType::TYPES) == 7);

}
