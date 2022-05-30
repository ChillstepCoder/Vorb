#include "stdafx.h"

#include "CityBuilder.h"
#include "City.h"
#include "city/CityQuartermaster.h"
#include "CityPlanner.h"
#include "BuildingBlueprint.h"

#include "World.h"
#include "resources/TileRepository.h"

#include "ecs/EntityComponentSystem.h"

#include "DebugRenderer.h"

#include "structure/StructureManager.h"

// TODO: replace?
#include "BuildingBlueprintGenerator.h"

CityBuilder::CityBuilder(City& city, World& world)
    : mCity(city)
    , mWorld(world)
{

}

void CityBuilder::update() {

    while (mBlueprintsToBuild.size()) {
        BuildingBlueprint* nextBp = mBlueprintsToBuild.front();
        // Try send this off to a contractor
        if (trySendBuildingJob(nextBp)) {
            mBlueprintsToBuild.pop_front();
        }
        else {
            break;
        }
    }
   
    // FILO queue right now
    while (mRoadsToBuild.size()) {
        debugBuildInstant(mRoadsToBuild.back());
        mRoadsToBuild.pop_back();
    }
}


void CityBuilder::addBlueprintToBuildAndPreprocess(BuildingBlueprint* blueprint) {
    assert(!blueprint->isBuilding);
    blueprint->isBuilding = true;
    preprocessBlueprint(blueprint);
    mBlueprintsToBuild.push_back(blueprint);
}

Building* CityBuilder::debugBuildInstant(BuildingBlueprint& bp, World& world) {

    PreciseTimer timer;
    const ui32v2& worldPos = bp.aabb.pos;

    static f32 BUILD_HEIGHTS[(int)BlueprintTileType::TYPES] = {
        0.0f, // NONE
        0.0f, // FLOOR
        0.0f, // DOOR
        1.0f, // WALL
    };
    static_assert(e_cast(BlueprintTileType::TYPES) == 4);

    // For mean height calc
    BitArray ownedTilesOnFirstFloor(bp.aabb.dims.x * bp.aabb.dims.y);
    for (ui32 y = 0; y < bp.aabb.dims.y; ++y) {
        for (ui32 x = 0; x < bp.aabb.dims.x; ++x) {
            const ui32 tileIndex = y * bp.aabb.dims.x + x;
            const BlueprintTileType type = bp.tiles[tileIndex].type;
            if (type != BlueprintTileType::NONE) {

                const TileID tileId = bp.tileIDs[e_cast(type)];
                if (tileId != TILE_ID_NONE) {
                    ownedTilesOnFirstFloor.setBitTo(tileIndex, true);
                }
            }
        }
    }

    // Clamp building height to 1 meter increments
    WorldGrid& grid = world.getWorldGrid();
    const ui32 meanHeight = round(grid.computeMeanHeightAtAABB(bp.aabb, ownedTilesOnFirstFloor));

    ui32 floorHeight = 3;
    ui32AABB3 aabb;
    aabb.x = bp.aabb.x;
    aabb.y = bp.aabb.y;
    aabb.z = meanHeight;
    aabb.width = bp.aabb.width;
    aabb.depth = bp.aabb.depth;
    aabb.height = bp.floorCount * floorHeight;

    // Allocate the building
    //PreciseTimer timer;
    Building* newBuilding = static_cast<Building*>(world.getStructureManager().makeNewStructure(StructureType::Building, aabb, floorHeight));
    newBuilding->mInteriorTilesInAABB.resizeAndZero(bp.aabb.dims.x * bp.aabb.dims.y * bp.floorCount);
    //std::cout << "New structure in " << timer.stop() << " ms\n";

    // === Flatten terrain ===
    //grid.flattenAABB(ui32AABB2(bp.bottomLeftWorldPos.x, bp.bottomLeftWorldPos.y, bp.dims.x, bp.dims.y), meanHeight);
    TileContainer& tileContainer = newBuilding->mTileContainer;

    // === Set world tiles, flatten heightmap, and track occupied bits ===
    ui32 tileIndex = 0;
    for (ui32 z = 0; z < bp.floorCount; ++z) {
        for (ui32 y = 0; y < bp.aabb.dims.y; ++y) {
            for (ui32 x = 0; x < bp.aabb.dims.x; ++x) {
                // TODO: Bitindex
                const BlueprintTileType type = bp.tiles[tileIndex].type;
                if (type != BlueprintTileType::NONE) {
                    // Flatten heightmap
                    if (z == 0) {
                        f32v2 tileWorldPos = worldPos + ui32v2(x, y);
                        grid.setHeightAt(tileWorldPos, meanHeight);
                    }

                    const TileID tileId = bp.tileIDs[e_cast(type)];
                    if (tileId != TILE_ID_NONE) {
                        const f32 height = meanHeight + (BUILD_HEIGHTS[e_cast(type)] + z) * tileContainer.getFloorHeight();
                        // TODO: Always ground??
                        newBuilding->mInteriorTilesInAABB.setBitTo(tileIndex, true);
                        TileIndex index = tileContainer.getTileIndexFromXYZOffset(x, y, z);
                        tileContainer.addTile(index, TileRepository::getTileData(tileId));
                        //assert(false); // Set building structure pointer
                        tileContainer.setTileGroundZPosition(index, height);

                        // TERRAIN
                        //TileHandle handle = world.getTileHandleAtWorldPos(tileWorldPos);
                        //TileContainer& container = *handle.getMutableContainer();
                        //container.addTile(handle.index, TileRepository::getTileData(tileId));
                        ////assert(false); // Set building structure pointer
                        //container.setTileGroundZPosition(handle.index, height);
                    }
                }
                ++tileIndex;
            }
        }
    }
    // Notify terrain data change (TODO: More precise, automatic)
    world.dirtyTerrainFromBrush(f32v2(newBuilding->mAABB.getCenter()), glm::length(f32v2(newBuilding->mAABB.dims)) * 0.5f);
    
    newBuilding->mGraph = std::move(bp.rooms);
    newBuilding->mFunction = bp.desc.function;
    newBuilding->mPlotIndex = bp.plotIndex;

    std::cout << "DebugBuildInstant " << timer.stop() << " ms\n";

    return newBuilding;
}

void CityBuilder::debugBuildInstant(RoadID roadId)
{
    static TileID bricksId = TileRepository::getTile("bricks1");
    static TileID grassId = TileRepository::getTile("grass1");

    CityRoad& road = *mCity.mRoads[roadId];
    TileID tileId = road.type == RoadType::PAVED ? bricksId : grassId;

    WorldGrid& grid = mWorld.getWorldGrid();
    ui32v2 xy;
    for (xy.y = road.aabb.y; xy.y < road.aabb.y + road.aabb.depth; ++xy.y) {
        for (xy.x = road.aabb.x; xy.x < road.aabb.x + road.aabb.width; ++xy.x) {
            TileHandle handle = mWorld.getTileHandleAtWorldPos(xy);
            handle.getMutableContainer()->addTile(handle.tileIndex, TileRepository::getTileData(tileId));
        }
    }
}

void CityBuilder::preprocessBlueprint(BuildingBlueprint* blueprint) {
    if (blueprint->flags & BuildingBlueprintFlags::BLUEPRINT_FLAG_CREATE_EARLY_STOCKPILE) {
        mCity.getCityQuartermaster().createStockpilesForBlueprint(*blueprint);
    }
}

bool CityBuilder::trySendBuildingJob(BuildingBlueprint* blueprint) {

    auto view = mWorld.getECS().mRegistry.view<BusinessBuildComponent>();
    bool success = false;
    // Find a business who can take on this build job
    for (auto entity : view) {
        auto& cmp = view.get<BusinessBuildComponent>(entity);
        // TODO: Bidding
        if (cmp.mCurrentBlueprint == nullptr) {
            cmp.mCurrentBlueprint = blueprint;
            success = true;
            break;
        }
    }
    return success;
}
