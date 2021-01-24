#include "stdafx.h"

#include "CityBuilder.h"
#include "City.h"
#include "CityPlanner.h"

#include "World.h"

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
    if (mInProgressPlans.empty()) {
        PlannedBuilding plan;
        if (mCity.getCityPlanner().recieveNextPlan(plan)) {
            mInProgressPlans.emplace_back(std::move(plan));
        }
    }

    // Build a building
    if (mInProgressPlans.size()) {
        PlannedBuilding& topPlan = mInProgressPlans.front();
        debugBuildInstant(topPlan);
        mInProgressPlans.pop_back();
    }
}

void CityBuilder::debugBuildInstant(PlannedBuilding& plan) {
    Building& building = plan.mBuilding;
    ui32v2 worldPos = building.mBottomLeftWorldPos;
    assert(building.mWallSegmentOffsets.size() % 2 == 0);
    int i = 0;

    static TileID wallId = TileRepository::getTile("rock1");

    // TODO: Height
    for (auto&& segmentXY : building.mWallSegmentOffsets) {
        const int xLen = segmentXY.x;
        const int yLen = segmentXY.y;

        // X wall
        int xOff = glm::sign(xLen);
        int dx = 0;
        while (dx != xLen) {
            mWorld.setTileAt(worldPos, Tile(wallId, TILE_ID_NONE, TILE_ID_NONE, 1));
            dx += xOff;
            worldPos.x += xOff;
            // TODO: Use height of building
        }

        // Y wall
        int yOff = glm::sign(yLen);
        int dy = 0;
        while (dy != yLen) {
            mWorld.setTileAt(worldPos, Tile(wallId, TILE_ID_NONE, TILE_ID_NONE, 1));
            dy += yOff;
            worldPos.y += yOff;
            // TODO: Use height of building
        }

        // Rooms and Floors

        // Doors

        // Furniture

    }

    // Register with the city
    mCity.mBuildings.emplace_back(std::move(building));
}
