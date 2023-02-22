#include "stdafx.h"

#include "CityBuilder.h"
#include "City.h"
#include "city/CityQuartermaster.h"
#include "CityPlanner.h"
#include "BuildingBlueprint.h"

#include "pathfinding/NavThread.h"
#include "pathfinding/NavWorld.h"

#include "world/IWorld.h"
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
    assert(!blueprint->isBuilding);
    blueprint->isBuilding = true;
    preprocessBlueprint(blueprint);
    mBlueprintsToBuild.push_back(blueprint);
}

Building* CityBuilder::debugBuildInstant(BuildingBlueprint& bp) {
    PROFILE_FUNCTION();
    assert(IS_GAME_THREAD());

    PreciseTimer timer;
    const i32v2& worldPos = bp.aabb.pos;

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
    IHeightmapGrid& grid = sWorld->getHeightmapGrid();
    const ui32 meanHeight = round(grid.computeMeanHeightAtAABB(bp.aabb, ownedTilesOnFirstFloor));

    ui32 floorHeight = 3;
    i32AABB3 aabb;
    aabb.x = bp.aabb.x;
    aabb.y = bp.aabb.y;
    aabb.z = meanHeight;
    aabb.width = bp.aabb.width;
    aabb.depth = bp.aabb.depth;
    aabb.height = bp.floorCount * floorHeight;

    // Allocate the building
    //PreciseTimer timer;
    Building* newBuilding = static_cast<Building*>(sWorld->getStructureManager().makeNewStructure(StructureType::Building, aabb, floorHeight));
    //std::cout << "New structure in " << timer.stop() << " ms\n";

    // === Flatten terrain ===
    //grid.flattenAABB(i32AABB2(bp.bottomLeftWorldPos.x, bp.bottomLeftWorldPos.y, bp.dims.x, bp.dims.y), meanHeight);
    TileContainer& tileContainer = *newBuilding->mTileContainer;
    tileContainer.allocateOwnedTiles();

    std::vector<Tile>& tiles = tileContainer.mTiles;
    std::vector<TileContainer*> dirtyNavTileContainers;

    // === Set world tiles, flatten heightmap, and track occupied bits ===
    ui32 tileIndex = 0;
    for (i32 z = 0; z < bp.floorCount; ++z) {
        for (i32 y = 0; y < bp.aabb.dims.y; ++y) {
            for (i32 x = 0; x < bp.aabb.dims.x; ++x, ++tileIndex) {
                // TODO: Bitindex
                const BlueprintTileType type = bp.tiles[tileIndex].type;
                if (type != BlueprintTileType::NONE) {
                    // Flatten heightmap
                    if (z == 0) {
                        f32v2 tileWorldPos = worldPos + i32v2(x, y);
                        // Epsilon to prevent z fighting
                        grid.setHeightAt(tileWorldPos, meanHeight - 0.005f);
                    }

                    tileContainer.setOwnedTile(tileIndex);

                    tileContainer.mWalls[tileIndex] = bp.walls[tileIndex];
                    // Stairs are processed below
                    if (type != BlueprintTileType::STAIRS) {
                        const TileID tileId = bp.tileIDs[e_cast(type)];
                        if (tileId != TILE_ID_NONE) {
                            Tile& tile = tiles[tileIndex];
                            // We dont add to mean height here because tile height is relative to the floor of this tile layer
                            //const f32 height = 0.0f;
                            const TileData& data = TileRepository::getTileData(tileId);
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
            const f32v3 tilePos = tileContainer.getTileXYZOffsetWithZScale(stairPiece.pos);
            // Place stair steps
            const f32 heightAdd = stairPiece.height * STAIR_TILE_HEIGHT;
            const f32 stairPieceBaseHeight = tilePos.z + heightAdd;
            Tile& tile = tiles[stairPiece.pos];
            tile.layers[e_cast(TileLayer::Main)] = stairPiece.isFlatPart ? stairsFlatTileId : stairsTileId;
            tile.setGroundZPosition(tilePos.z + heightAdd);
            tile.setOrientation(stairPiece.dir, TileLayer::Main);
            tileContainer.onTileChanged(stairPiece.pos);
        }
    }

    // Notify terrain data change (TODO: More precise, automatic)
    sWorld->dirtyTerrainFromBrush(f32v2(newBuilding->mAABB.getCenter()), glm::length(f32v2(newBuilding->mAABB.dims)) * 0.5f);
    
    finishBuilding(*newBuilding, bp);

    return newBuilding;
}

void CityBuilder::debugBuildRoadInstant(RoadID roadId)
{
    static TileID bricksId = TileRepository::getTile(StrToken("bricks1"));
    static TileID grassId = TileRepository::getTile(StrToken("grass1"));

    CityRoad& road = *mCity.mRoads[roadId];
    TileID tileId = road.type == RoadType::PAVED ? bricksId : grassId;

    i32v2 xy;
    for (xy.y = road.aabb.y; xy.y < road.aabb.y + road.aabb.depth; ++xy.y) {
        for (xy.x = road.aabb.x; xy.x < road.aabb.x + road.aabb.width; ++xy.x) {
            TileHandle handle = sWorld->getTerrainTileHandleAtWorldPos(xy);
            handle.getMutableContainer()->setTileLayer(handle.tileIndex, TileRepository::getTileData(tileId));
        }
    }
}

void CityBuilder::preprocessBlueprint(BuildingBlueprint* blueprint) {
    if (blueprint->flags.isBitSet(BuildingBlueprintFlags::BLUEPRINT_FLAG_CREATE_EARLY_STOCKPILE)) {
        mCity.getCityQuartermaster().createStockpilesForBlueprint(*blueprint);
    }
}

void CityBuilder::finishBuilding(Building& building, BuildingBlueprint& blueprint) {
    building.mFunction = blueprint.desc.function;
    building.mPlotIndex = blueprint.plotIndex;
    building.mDoorTiles = blueprint.exteriorDoors;
    assert(building.mDoorTiles.size());
    assert(building.mRooms.size());

    // Mark ready for access
    building.getTileContainer()->setState(TileContainerState::READY);

    // Navmesh
    if (sNavWorld) {
        sNavWorld->markContainerNavDirty(building.mTileContainer);
    }

    sBuildingMesher.buildMeshAndPhysicsAsync(building);
}

bool CityBuilder::trySendBuildingJob(BuildingBlueprint* blueprint) {

    auto view = sWorld->getECS().mRegistry.view<BusinessBuildComponent>();
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
