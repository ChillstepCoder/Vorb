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

void CityBuilder::addUrgentPlans(PlannedBuilding&& plannedBuilding)
{
    assert(false);
}

void CityBuilder::update()
{
    // Grab new plans
    if (mInProgressBlueprints.empty()) {
        if (std::unique_ptr<BuildingBlueprint> bp = mCity.getCityPlanner().recieveNextBlueprint()) {
            debugBuildInstant(*bp);
            mInProgressBlueprints.emplace_back(std::move(bp));
        }
    }

    // Build a building
    //if (mInProgressBlueprints.size()) {
    //    // TODO: Pop
    //    BuildingBlueprint& bp = *mInProgressBlueprints.back();
    //    //mInProgressBlueprints.pop_back();
    //    debugBuildInstant(bp);
    //}
}

void CityBuilder::debugBuildInstant(BuildingBlueprint& bp) {
    //Building& building = plan.mBuilding;
    //ui32v2 worldPos = building.mBottomLeftWorldPos;
    //assert(building.mWallSegmentOffsets.size() % 2 == 0);
    //int i = 0;

    //static TileID wallId = TileRepository::getTile("rock1");

    //// TODO: Height
    //for (auto&& segmentXY : building.mWallSegmentOffsets) {
    //    const int xLen = segmentXY.x;
    //    const int yLen = segmentXY.y;

    //    // X wall
    //    int xOff = glm::sign(xLen);
    //    int dx = 0;
    //    while (dx != xLen) {
    //        mWorld.setTileAt(worldPos, Tile(wallId, TILE_ID_NONE, TILE_ID_NONE, 1));
    //        dx += xOff;
    //        worldPos.x += xOff;
    //        // TODO: Use height of building
    //    }

    //    // Y wall
    //    int yOff = glm::sign(yLen);
    //    int dy = 0;
    //    while (dy != yLen) {
    //        mWorld.setTileAt(worldPos, Tile(wallId, TILE_ID_NONE, TILE_ID_NONE, 1));
    //        dy += yOff;
    //        worldPos.y += yOff;
    //        // TODO: Use height of building
    //    }

    //    // Rooms and Floors

    //    // Doors

    //    // Furniture

    //}

    static TileID wallId = TileRepository::getTile("rock1");
    static TileID bricksId = TileRepository::getTile("bricks1");

    ui32v2 worldPos = bp.rootWorldPos;
    worldPos.y -= bp.dims.y / 2;

    for (int y = 0; y < bp.dims.y; ++y) {
        for (int x = 0; x < bp.dims.x; ++x) {
            const int index = y * bp.dims.x + x;
            BlueprintTileType type = bp.tiles[index].type;
            if (type == BlueprintTileType::FLOOR) {
                mWorld.setTileAt(worldPos + ui32v2(x, y), Tile(bricksId, TILE_ID_NONE, TILE_ID_NONE, 0));
            }
            else if (type == BlueprintTileType::WALL) {
                mWorld.setTileAt(worldPos + ui32v2(x, y), Tile(wallId, TILE_ID_NONE, TILE_ID_NONE, 1));
            }
        }
    }

    // Register with the city
    Building newBuilding;
    mCity.mBuildings.emplace_back(std::move(newBuilding));
}
