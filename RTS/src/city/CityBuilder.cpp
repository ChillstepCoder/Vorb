#include "stdafx.h"

#include "CityBuilder.h"
#include "City.h"
#include "city/CityQuartermaster.h"
#include "CityPlanner.h"
#include "BuildingBlueprint.h"
#include "tile/TileContainerRepository.h"

#include "pathfinding/NavThread.h"
#include "pathfinding/NavWorld.h"

#include "world/IWorld.h"
#include "world/IHeightmapGrid.h"
#include "world/srv/SrvWorldInterface.h"
#include "resources/TileRepository.h"

#include "ecs/IEntityComponentSystem.h"

#include "debugging/DebugRenderer.h"
#include "rendering/mesh/mesher/BuildingMesher.h"

#include "structure/StructureManager.h"

// TODO: replace?
#include "BuildingBlueprintGenerator.h"



CityBuilder::CityBuilder(City& city)
    : mCity(city)
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
        debugBuildRoadInstant(mRoadsToBuild.back());
        mRoadsToBuild.pop_back();
    }
}

void CityBuilder::addBlueprintToBuildAndPreprocess(BuildingBlueprint* blueprint) {
    assert(!blueprint->isBuilding);
    blueprint->isBuilding = true;
    preprocessBlueprint(*blueprint);
    mBlueprintsToBuild.push_back(blueprint);
}

Building* CityBuilder::debugBuildInstant(BuildingBlueprint& bp) {
    PROFILE_FUNCTION();
    ASSERT_GAME_THREAD();
    assert(bp.world);

    PreciseTimer timer;
    const i32v2& worldPos = bp.mTileSpatialGrid.getWorldPos2D();

    BitArray& tilesNeedingTerrainFlatten = bp.tilesNeedingTerrainFlatten;

    // Clamp building height to 1 meter increments
    IHeightmapGrid& grid = bp.world->getHeightmapGrid();
    const ui32 meanHeight = round(grid.computeMeanHeightAtAABB(bp.mTileSpatialGrid.getAABB(), tilesNeedingTerrainFlatten));

    // Allocate the building
    //PreciseTimer timer;
    Building* newBuilding = static_cast<Building*>(bp.world->getStructureManager().makeNewStructure(StructureType::Building, bp.mTileSpatialGrid.getAABB(), bp.mTileSpatialGrid.getFloorHeight()));
    //std::cout << "New structure in " << timer.stop() << " ms\n";

    // === Flatten terrain ===
    //grid.flattenAABB(i32AABB2(bp.bottomLeftWorldPos.x, bp.bottomLeftWorldPos.y, bp.dims.x, bp.dims.y), meanHeight);
    TileContainer& tileContainer = *newBuilding->mTileContainer;
    tileContainer.allocateOwnedTiles();

    std::vector<Tile>& tiles = tileContainer.mTiles;
    std::vector<TileContainer*> dirtyNavTileContainers;
    const i32v3& dims = bp.mTileSpatialGrid.getDims();

    // === Set world tiles, flatten heightmap, and track occupied bits ===
    ui32 tileIndex = 0;
    for (i32 z = 0; z < dims.z; ++z) {
        for (i32 y = 0; y < dims.y; ++y) {
            for (i32 x = 0; x < dims.x; ++x, ++tileIndex) {
                // TODO: Bitindex
                const BlueprintTileType type = bp.tiles[tileIndex];
                
                // Copy walls
                tileContainer.mTileWallsContainer.setSouthWallAtTile(tileIndex, bp.walls.getSouthWallAtTile(tileIndex));
                tileContainer.mTileWallsContainer.setWestWallAtTile(tileIndex, bp.walls.getWestWallAtTile(tileIndex));
                if (type != BlueprintTileType::NONE) {
                    // Flatten heightmap
                    if (z == 0) {
                        f32v2 tileWorldPos = worldPos + i32v2(x, y);
                        // Epsilon to prevent z fighting
                        grid.setHeightAtWorldPos(tileWorldPos, meanHeight - 0.005f);
                    }

                    tileContainer.setOwnedTile(tileIndex);
                    // Stairs are processed below
                    if (type != BlueprintTileType::STAIRS) {
                        const TileID tileId = bp.tileIDs[e_cast(type)];
                        if (tileId != TILE_ID_NONE) {
                            Tile& tile = tiles[tileIndex];
                            // We dont add to mean height here because tile height is relative to the floor of this tile layer
                            //const f32 height = 0.0f;
                            const TileDef& data = TileRepository::getTileData(tileId);
                            tile.layers[data.layer] = data.id;
                            //tile.groundZOffset = height;
                            //assert(false); // Set building structure pointer
                            // TODO: always set ground position?
                        }
                        tileContainer.onTileChanged(tileIndex);
                    }
                    // INTERSECT TERRAIN
                    // TODO: Intersect terrain
                    //TileHandle handle = world.getTileHandleAtWorldPos(tileWorldPos);
                    //TileContainer& container = *handle.getMutableContainer();
                    //container.addTile(handle.index, TileRepository::getTileData(tileId));
                    ////assert(false); // Set building structure pointer
                    //container.setTileGroundZPosition(handle.index, height);
                }
            }
        }
    }

    // Copy room data
    newBuilding->mRooms = std::move(bp.rooms);

    // Set stairs tiles
    TileID stairsTileId = bp.tileIDs[e_cast(BlueprintTileType::STAIRS)];
    TileID stairsFlatTileId = bp.tileIDs[e_cast(BlueprintTileType::STAIRS_FLAT)];
    for (auto& stairsVec : bp.stairs) {
        for (auto& stairPiece : stairsVec) {
            const f32v3 tilePos = tileContainer.getTileSpatialGrid().getTileXYZOffsetWithZScale(stairPiece.pos);
            // Place stair steps
            const f32 heightAdd = stairPiece.height * STAIR_TILE_HEIGHT;
            const f32 stairPieceBaseHeight = tilePos.z + heightAdd;
            Tile& tile = tiles[stairPiece.pos];
            tile.layers[e_cast(TileLayer::Main)] = stairPiece.isFlatPart ? stairsFlatTileId : stairsTileId;
            tile.setGroundZOffset(tilePos.z + heightAdd);
            tile.setOrientation(stairPiece.dir, TileLayer::Main);
            tileContainer.onTileChanged(stairPiece.pos);
        }
    }

    TileContainerEvent loadFinishedEvent;
    loadFinishedEvent.container = &tileContainer;
    tileContainer.getWorld().getTileContainerRepository().dispatchLoadFinished(loadFinishedEvent);

    finishBuilding(*newBuilding, bp);

    return newBuilding;
}

void CityBuilder::debugBuildRoadInstant(RoadID roadId)
{
    static TileID bricksId = TileRepository::getTile(StrToken("bricks", 1));

    CityRoad& road = *mCity.mRoads[roadId];
    // TODO: Other types of paths
    if (road.type != RoadType::PAVED) {
        LOG_WARN("Invalid road type built");
        return;
    }
    TileID tileId = bricksId;

    i32v2 xy;
    for (xy.y = road.aabb.y; xy.y < road.aabb.y + road.aabb.depth; ++xy.y) {
        for (xy.x = road.aabb.x; xy.x < road.aabb.x + road.aabb.width; ++xy.x) {
            TileHandle handle = mCity.getWorld().getTerrainTileHandleAtWorldPos(xy);
            handle.getMutableContainer()->setTileLayer(handle.tileIndex, TileRepository::getTileData(tileId));
        }
    }
}

void CityBuilder::preprocessBlueprint(BuildingBlueprint& bp) {
    ASSERT_GAME_THREAD();
    assert(bp.world == &mCity.getWorld());

    // Clamp building height to 1 meter increments
    IHeightmapGrid& grid = mCity.getWorld().getHeightmapGrid();
    const ui32 meanHeight = round(grid.computeMeanHeightAtAABB(bp.mTileSpatialGrid.getAABB(), bp.tilesNeedingTerrainFlatten));

    bp.building = static_cast<Building*>(mCity.getWorld().getStructureManager().makeNewStructure(StructureType::Building, bp.mTileSpatialGrid.getAABB(), bp.mTileSpatialGrid.getFloorHeight()));
    // Force ready so we can place tiles
    bp.building->getTileContainer()->setState(TileContainerState::READY);
    if (bp.flags.isBitSet(BuildingBlueprintFlags::BLUEPRINT_FLAG_CREATE_EARLY_STOCKPILE)) {
        mCity.getCityQuartermaster().createStockpilesForBlueprint(bp);
    }
}

void CityBuilder::finishBuilding(Building& building, BuildingBlueprint& blueprint) {
    building.mFunction = blueprint.desc->function;
    building.mPlotIndex = blueprint.plotIndex;
    building.mDoorTiles = blueprint.exteriorDoors;
    assert(building.mDoorTiles.size());
    assert(building.mRooms.size());

    // Mark ready for access
    building.getTileContainer()->setState(TileContainerState::READY);

    // Navmesh
    SrvWorldInterface* srvWorldInterface = dynamic_cast<SrvWorldInterface*>(blueprint.world);
    if (srvWorldInterface) {
        srvWorldInterface->getNavWorld().markContainerNavDirty(building.mTileContainer);
    }

}

bool CityBuilder::trySendBuildingJob(BuildingBlueprint* blueprint) {
    assert(blueprint->world == &mCity.getWorld());

    auto view = mCity.getWorld().getECS().mRegistry.view<BusinessBuildComponent>();
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
