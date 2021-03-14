#include "stdafx.h"

#include "CityBuilder.h"
#include "City.h"
#include "CityPlanner.h"

#include "World.h"

// TODO: replace?
#include "BuildingBlueprintGenerator.h"

CityBuilder::CityBuilder(City& city, World& world)
    : mCity(city)
    , mWorld(world)
{

}

void CityBuilder::update()
{
    // Grab new plans
    //if (mInProgressBlueprints.empty()) {
        if (std::unique_ptr<BuildingBlueprint> bp = mCity.getCityPlanner().recieveNextBlueprint()) {
            debugBuildInstant(*bp);
            mInProgressBlueprints.emplace_back(std::move(bp));
        }
    //}

}

void CityBuilder::debugBuildInstant(BuildingBlueprint& bp) {

    static TileID wallId = TileRepository::getTile("rock1");
    static TileID bricksId = TileRepository::getTile("bricks1");
    static TileID doorId = TileRepository::getTile("door");

    ui32v2 worldPos = bp.bottomLeftWorldPos;

    static TileID BUILD_TILES[(int)BlueprintTileType::TYPES] = {
        TILE_ID_NONE,// NONE
        bricksId,// FLOOR
        doorId,// DOOR
        wallId,// WALL
    };
    static int BUILD_HEIGHTS[(int)BlueprintTileType::TYPES] = {
        0, // NONE
        0, // FLOOR
        0, // DOOR
        0, // WALL
    };

    for (int y = 0; y < bp.dims.y; ++y) {
        for (int x = 0; x < bp.dims.x; ++x) {
            const int index = y * bp.dims.x + x;
            const BlueprintTileType type = bp.tiles[index].type;
            const TileID tile = BUILD_TILES[enum_cast(type)];
            if (tile != TILE_ID_NONE) {
                const int height = BUILD_HEIGHTS[enum_cast(type)];
                mWorld.setTileAt(worldPos + ui32v2(x, y), Tile(tile, TILE_ID_NONE, TILE_ID_NONE, height));
            }
        }
    }

    // Register with the city
    Building newBuilding;
    mCity.mBuildings.emplace_back(std::move(newBuilding));
}
