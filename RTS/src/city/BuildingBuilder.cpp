#include "stdafx.h"

#include "BuildingBuilder.h"
#include "building/BuildingBlueprint.h"
#include "tile/TileContainerRepository.h"
#include "tile/TileContainerLoader.h"

#include "pathfinding/NavThread.h"
#include "pathfinding/NavWorld.h"

#include "world/World.h"
#include "world/IHeightmapGrid.h"
#include "resources/TileRepository.h"

#include "ecs/IFullECS.h"

#include "debugging/DebugRenderer.h"
#include "rendering/mesh/mesher/BuildingMesher.h"

#include "building/BuildingGrid.h"

// TODO: replace?
#include "building/BuildingBlueprintGenerator.h"

Building* BuildingBuilder::debugCreateAndBuildNewBuilding(World& world, std::unique_ptr<BuildingBlueprint>& bpPtr) {
    PROFILE_FUNCTION();
    ASSERT_GAME_THREAD();

    TileRepository& tileRepo = TileRepository::get();

    PreciseTimer timer;
    const i32v2 worldPos = bpPtr->worldPosRootDTile.toTilePos();
    const i32AABB2 aabb(worldPos, bpPtr->dimsDTile.toTilePos());

    BitArray tilesNeedingTerrainFlatten = bpPtr->computeSolidTilesFirstFloor();

    // Clamp building height to 1 meter increments
    IHeightmapGrid& grid = world.getHeightmapGrid();
    const ui32 meanHeight = round(grid.computeMeanHeightAtAABB(aabb, tilesNeedingTerrainFlatten));
    const i32AABB3 aabb3d(i32v3(aabb.pos.x, aabb.pos.y, meanHeight), i32v3(aabb.dims.x, aabb.dims.y, bpPtr->floorCount * bpPtr->floorHeight));
    // Allocate the building
    //PreciseTimer timer;
    Building* newBuilding = static_cast<Building*>(world.getBuildingGrid().debugMakeNewFullyBuiltBuilding(aabb3d, bpPtr->floorHeight, bpPtr->ownedDTiles, bpPtr));
    if (!newBuilding) {
        return nullptr;
    }

    return newBuilding;
}
