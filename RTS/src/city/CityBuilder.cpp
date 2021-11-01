#include "stdafx.h"

#include "CityBuilder.h"
#include "City.h"
#include "CityPlanner.h"
#include "BuildingBlueprint.h"

#include "World.h"
#include "world/TileRepository.h"

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
    if (mWaitingBlueprints.empty()) {
        if (std::unique_ptr<BuildingBlueprint> bp = mCity.getCityPlanner().recieveNextBlueprint()) {
            //mWaitingBlueprints.push_front(std::move(bp));
            debugBuildInstant(*bp);
        }
    }

    // FILO queue right now
    while (mRoadsToBuild.size()) {
        debugBuildInstant(mRoadsToBuild.back());
        mRoadsToBuild.pop_back();
    }
}

BuildingBlueprint* CityBuilder::aquireBlueprintToBuild(entt::entity businessId) {
    // TODO: Priority?
    if (mWaitingBlueprints.empty()) {
        return nullptr;
    }
    mInProgressBlueprints.emplace_back(std::make_pair(std::move(mWaitingBlueprints.front()), businessId));
    mWaitingBlueprints.pop_front();
    return mInProgressBlueprints.back().first.get();
}

void CityBuilder::onBlueprintComplete(BuildingBlueprint* bp)
{
    for (size_t i = 0; i < mInProgressBlueprints.size(); ++i) {
        if (mInProgressBlueprints[i].first.get() == bp) {
            mInProgressBlueprints[i] = std::move(mInProgressBlueprints.back());
            mInProgressBlueprints.pop_back();
            return;
        }
    }
    assert(false); // Failed to find blueprint
}

void CityBuilder::debugBuildInstant(BuildingBlueprint& bp) {


    ui32v2 worldPos = bp.bottomLeftWorldPos;

   
    static int BUILD_HEIGHTS[(int)BlueprintTileType::TYPES] = {
        0, // NONE
        0, // FLOOR
        0, // DOOR
        2, // WALL
    };
    static_assert(enum_cast(BlueprintTileType::TYPES) == 4);

    for (int y = 0; y < bp.dims.y; ++y) {
        for (int x = 0; x < bp.dims.x; ++x) {
            const int index = y * bp.dims.x + x;
            const BlueprintTileType type = bp.tiles[index].type;
            const TileID tile = bp.tileIDs[enum_cast(type)];
            if (tile != TILE_ID_NONE) {
                const int height = BUILD_HEIGHTS[enum_cast(type)];
                mWorld.setTileAt(worldPos + ui32v2(x, y), Tile(tile, TILE_ID_NONE, TILE_ID_NONE, height));
            }
        }
    }

    // Register with the city
    Building newBuilding;
    // TODO: This is an expensive copy
    newBuilding.mGraph = bp.nodes;
    newBuilding.mFunction = bp.desc.function;
    newBuilding.mPlotIndex = bp.plotIndex;
    mCity.addCompletedBuilding(std::move(newBuilding));
}

void CityBuilder::debugBuildInstant(RoadID roadId)
{
    static TileID bricksId = TileRepository::getTile("bricks1");
    static TileID grassId = TileRepository::getTile("grass1");

    CityRoad& road = *mCity.mRoads[roadId];
    TileID tileId = road.type == RoadType::PAVED ? bricksId : grassId;

    for (ui32 y = road.aabb.y; y < road.aabb.y + road.aabb.height; ++y) {
        for (ui32 x = road.aabb.x; x < road.aabb.x + road.aabb.width; ++x) {
            mWorld.setTileAt(ui32v2(x, y), Tile(tileId, TILE_ID_NONE, TILE_ID_NONE, 0));
        }
    }
}
