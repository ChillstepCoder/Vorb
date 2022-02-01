#include "stdafx.h"

#include "CityBuilder.h"
#include "City.h"
#include "CityPlanner.h"
#include "BuildingBlueprint.h"

#include "World.h"
#include "world/TileRepository.h"

#include "ecs/EntityComponentSystem.h"

#include "DebugRenderer.h"

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


void CityBuilder::addBlueprintToBuild(BuildingBlueprint* blueprint) {
    assert(!blueprint->isBuilding);
    blueprint->isBuilding = true;
    mBlueprintsToBuild.push_back(blueprint);
}

void CityBuilder::debugBuildInstant(BuildingBlueprint& bp) {

    ui32v2 worldPos = bp.bottomLeftWorldPos;

    static f32 BUILD_HEIGHTS[(int)BlueprintTileType::TYPES] = {
        0.0f, // NONE
        0.0f, // FLOOR
        0.0f, // DOOR
        3.0f, // WALL
    };
    static_assert(enum_cast(BlueprintTileType::TYPES) == 4);

    // Register with the city
    Building newBuilding;
    newBuilding.mAABB.pos = bp.bottomLeftWorldPos;
    newBuilding.mAABB.dims = bp.dims;
    newBuilding.mOwnedTilesInAABB.resizeAndZero(bp.dims.x * bp.dims.y);

    // === Flatten terrain ===
    // Compute mean height of height grid
    f32 meanHeight = 0.0f;
    ui32 total = 0;
    WorldGrid& grid = mWorld.getWorldGrid();
    for (ui32 y = 0; y < bp.dims.y; ++y) {
        for (ui32 x = 0; x < bp.dims.x; ++x) {
            const ui32 index = y * bp.dims.x + x;
            const BlueprintTileType type = bp.tiles[index].type;
            if (type != BlueprintTileType::NONE) {
                f32v2 pos(worldPos.x + x + 0.5f, worldPos.y + y + 0.5f);
                f32 h;
                if (grid.tryComputeHeightAtPoint(pos, &h)) {
                    meanHeight += h;
                    ++total;
                }
                else {
                    assert(false); // Failed to compute height for city builder debug build instant
                }
            }
        }
    }
    meanHeight /= (f32)total;

    // Flatten heightmap
    //grid.flattenAABB(ui32AABB2(bp.bottomLeftWorldPos.x, bp.bottomLeftWorldPos.y, bp.dims.x, bp.dims.y), meanHeight);

    // === Set world tiles, flatten heightmap, and track occupied bits ===
    for (ui32 y = 0; y < bp.dims.y; ++y) {
        for (ui32 x = 0; x < bp.dims.x; ++x) {
            const ui32 index = y * bp.dims.x + x;
            const BlueprintTileType type = bp.tiles[index].type;
            if (type != BlueprintTileType::NONE) {
                // Flatten heightmap
                f32v2 tileWorldPos = worldPos + ui32v2(x, y);
                grid.setHeightAt(tileWorldPos, meanHeight);

                const TileID tileId = bp.tileIDs[enum_cast(type)];
                if (tileId != TILE_ID_NONE) {
                    const f32 height = BUILD_HEIGHTS[enum_cast(type)] + meanHeight;
                    // TODO: Always ground??
                    newBuilding.mOwnedTilesInAABB.setBitTo(index, true);
                    TileHandle handle = mWorld.getTileHandleAtWorldPos(tileWorldPos);
                    Chunk* chunk = handle.getMutableChunk();
                    chunk->addTile(handle.index, TileRepository::getTileData(tileId));
                    chunk->setTileFlag(handle.index, TILE_FLAG_IS_BUILDING);
                    chunk->setTileBaseZPosition(handle.index, height);
                }
            }
        }
    }

    // Notify terrain data change (TODO: More precise, automatic)
    mWorld.dirtyTerrainFromBrush(f32v2(newBuilding.mAABB.getCenter()), glm::length(f32v2(newBuilding.mAABB.dims)) * 0.5f);
    
    newBuilding.mZPosFloor = meanHeight;
    newBuilding.mZPosRoof = meanHeight + 3.0005f;
    newBuilding.mGraph = std::move(bp.nodes);
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

    WorldGrid& grid = mWorld.getWorldGrid();
    ui32v2 xy;
    for (xy.y = road.aabb.y; xy.y < road.aabb.y + road.aabb.height; ++xy.y) {
        for (xy.x = road.aabb.x; xy.x < road.aabb.x + road.aabb.width; ++xy.x) {
            mWorld.addTile(xy, TileRepository::getTileData(tileId));
        }
    }
}

bool CityBuilder::trySendBuildingJob(BuildingBlueprint* blueprint) {

    auto view = mWorld.getECS().mRegistry.view<BusinessBuildComponent>();
    bool success = false;
    // Update businesses
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
