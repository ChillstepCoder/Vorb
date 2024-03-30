#include "stdafx.h"

#include "CityBuilder.h"
#include "City.h"
#include "city/CityQuartermaster.h"
#include "CityPlanner.h"
#include "world/settlement/building/BuildingBlueprint.h"
#include "tile/TileContainerRepository.h"

#include "pathfinding/NavThread.h"
#include "pathfinding/NavWorld.h"

#include "world/World.h"
#include "world/IHeightmapGrid.h"
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
    assert(false);
    /*  assert(!blueprint->isBuilding);
      blueprint->isBuilding = true;
      preprocessBlueprint(*blueprint);
      mBlueprintsToBuild.push_back(blueprint);*/
}

Building* CityBuilder::debugBuildInstant(World& world, BuildingBlueprint& bp) {
    PROFILE_FUNCTION();
    ASSERT_GAME_THREAD();

    TileRepository& tileRepo = TileRepository::get();

    PreciseTimer timer;
    const i32v2 worldPos = bp.worldPosRootDTile.toTilePos();
    const i32AABB2 aabb(worldPos, bp.dimsDTile * DTILE_WIDTH);

    BitArray tilesNeedingTerrainFlatten = bp.computeSolidTilesFirstFloor();

    // Clamp building height to 1 meter increments
    IHeightmapGrid& grid = world.getHeightmapGrid();
    const ui32 meanHeight = round(grid.computeMeanHeightAtAABB(aabb, tilesNeedingTerrainFlatten));
    const i32AABB3 aabb3d(i32v3(aabb.pos.x, aabb.pos.y, meanHeight), i32v3(aabb.dims.x, aabb.dims.y, bp.floorCount * bp.floorHeight));
    // Allocate the building
    //PreciseTimer timer;
    Building* newBuilding = static_cast<Building*>(world.getStructureManager().makeNewStructure(StructureType::Building, aabb3d, bp.floorHeight));
    //std::cout << "New structure in " << timer.stop() << " ms\n";

    // === Flatten terrain ===
    //grid.flattenAABB(i32AABB2(bp.bottomLeftWorldPos.x, bp.bottomLeftWorldPos.y, bp.dims.x, bp.dims.y), meanHeight);
    TileContainer& tileContainer = *newBuilding->mTileContainer;
    tileContainer.allocateOwnedTiles();

    std::vector<Tile>& tiles = tileContainer.mTiles;
    std::vector<TileContainer*> dirtyNavTileContainers;
    const i32v3 dims(aabb.dims.x, aabb.dims.y, bp.floorCount);
    const i32 floorStride = dims.x * dims.y;

    // === Set world tiles, flatten heightmap, and track occupied bits ===
    for (ui32 i = 0; i < bp.tileTargetCount; ++i) {
        BuildingBlueprintTileTarget& tileTarget = bp.tileTargets[i];
        if (tileTarget.tileIndex < floorStride) {
            i32v2 tileWorldPos = worldPos + i32v2(tileTarget.tileIndex % dims.x, tileTarget.tileIndex / dims.x);
            // Epsilon to prevent z fighting
            grid.setHeightAtWorldPos(tileWorldPos, meanHeight - 0.005f);
        }

        assert(isTileValid(tileTarget.id));
        Tile& tile = tiles[tileTarget.tileIndex];
        const TileDef& data = tileRepo.getLoadedOrUnloadedAsset(tileTarget.id);
        tile.layers[data.layer] = tileTarget.id;
        tileContainer.onTileChanged(tileTarget.tileIndex);
        tileContainer.setOwnedTile(tileTarget.tileIndex); // TODO: OwnedDTiles
    }
    for (ui32 i = 0; i < bp.wallTargetCount; ++i) {
        BuildingBlueprintWallTarget& wallTarget = bp.wallTargets[i];
        assert(isTileValid(wallTarget.id));
        TileWall newWall{ .wallID = wallTarget.id, .isDoor = false /*TODO: this is wrong...*/ };
        tileContainer.mTileWallsContainer.setWallAtTile(wallTarget.tileIndex, newWall, wallTarget.dir);
    }

    // Copy room data
    //newBuilding->mRooms = std::move(bp.rooms);

    // Set stairs tiles
    TileID stairsTileId = bp.stairsTileID;
    TileID stairsFlatTileId = bp.stairsFlatTileID;
    for (i32 i = 0; i < bp.stairPieceCount; ++i) {
        StairPiece& stairPiece = bp.stairPieces[i];
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

    TileContainerEvent loadFinishedEvent;
    loadFinishedEvent.container = &tileContainer;
    tileContainer.getWorld().getTileContainerRepository().dispatchLoadFinished(loadFinishedEvent);

    finishBuilding(world, *newBuilding, bp);

    return newBuilding;
}

void CityBuilder::debugBuildRoadInstant(RoadID roadId)
{
    static TileID bricksId = TileRepository::get().getTileID(CStrToken("bricks"));

    CityRoad& road = *mCity.mRoads[roadId];
    // TODO: Other types of paths
    if (road.type != OLDRoadType::PAVED) {
        LOG_WARN("Invalid road type built");
        return;
    }
    TileID tileId = bricksId;

    TileRepository& tileRepo = TileRepository::get();

    i32v2 xy;
    for (xy.y = road.aabb.y; xy.y < road.aabb.y + road.aabb.depth; ++xy.y) {
        for (xy.x = road.aabb.x; xy.x < road.aabb.x + road.aabb.width; ++xy.x) {
            TileHandle handle = mCity.getWorld().getTerrainTileHandleAtWorldPos(xy);
            handle.getMutableContainer()->setTileLayer(handle.tileIndex, tileRepo.getLoadedOrUnloadedAsset(tileId));
        }
    }
}

void CityBuilder::preprocessBlueprint(BuildingBlueprint& bp) {
    ASSERT_GAME_THREAD();
    assert(false);
    // Clamp building height to 1 meter increments
   // IHeightmapGrid& grid = mCity.getWorld().getHeightmapGrid();
   // const ui32 meanHeight = round(grid.computeMeanHeightAtAABB(bp.mTileSpatialGrid.getAABB(), bp.solidTilesFirstFloor));
    //assert(false);
    //bp.building = static_cast<Building*>(mCity.getWorld().getStructureManager().makeNewStructure(StructureType::Building, bp.mTileSpatialGrid.getAABB(), bp.mTileSpatialGrid.getFloorHeight()));
    //// Force ready so we can place tiles
    //bp.building->getTileContainer()->setState(TileContainerState::READY);
    //if (bp.flags.isBitSet(BuildingBlueprintFlags::BLUEPRINT_FLAG_CREATE_EARLY_STOCKPILE)) {
    //    mCity.getCityQuartermaster().createStockpilesForBlueprint(bp);
    //}
}

void CityBuilder::finishBuilding(World& world, Building& building, BuildingBlueprint& blueprint) {
    building.mFunction = blueprint.desc->function;
    //building.mDoorTiles = blueprint.exteriorDoors;
    //assert(building.mDoorTiles.size());
    //assert(building.mRooms.size());

    // Mark ready for access
    building.getTileContainer()->setState(TileContainerState::READY);

    // Navmesh
    if (NavWorld* navWorld = world.tryGetNavWorld()) {
        navWorld->markContainerNavDirty(building.mTileContainer);
    }

}

bool CityBuilder::trySendBuildingJob(BuildingBlueprint* blueprint) {
    auto view = mCity.getWorld().getECS().mRegistry.view<BusinessBuildComponent>();
    bool success = false;
    // Find a business who can take on this build job
    for (auto entity : view) {
        auto& cmp = view.get<BusinessBuildComponent>(entity);
        // TODO: Bidding
        if (cmp.mCurrentBlueprint == nullptr) {
            assert(false);
            //cmp.mCurrentBlueprint = blueprint;
            success = true;
            break;
        }
    }
    return success;
}
