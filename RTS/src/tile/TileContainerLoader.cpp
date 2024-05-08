#include "stdafx.h"
#include "TileContainerLoader.h"

#include "world/World.h"
#include "world/IHeightmapGrid.h"
#include "world/IChunkGrid.h"
#include "world/Chunk.h"
#include "world/ecosystem/FishEcosystem.h"
#include "building/building.h"
#include "building/BuildingBlueprint.h"
#include "pathfinding/NavWorld.h"
#include "generation/ChunkGenerator.h"

#include "gamethread/GameThreadTasks.h"

#include "tile/TileContainer.h"
#include "tile/TileContainerRepository.h"

#include "tile/TileContainerLoader.h"
#include "resources/TileRepository.h"

TileContainerLoader::TileContainerLoader(World& world) : mWorld(world) {
    initEvents();
}

void TileContainerLoader::loadBuilding(Building& building) const {
    if (building.getBlueprint()) {
        loadBuildingFromBlueprint(building);
    }
    else {
        assert(false);
    }
}

void TileContainerLoader::loadBuildingFromBlueprint(Building& building) const {
    PROFILE_FUNCTION();
    ASSERT_GAME_THREAD();

    TileRepository& tileRepo = TileRepository::get();
    BuildingBlueprint& bp = *building.getBlueprint();

    PreciseTimer timer;
    const i32v2 worldPos = bp.worldPosRootDTile.toTilePos();
    const i32AABB2 aabb(worldPos, bp.dimsDTile * DTILE_WIDTH);

    // Clamp building height to 1 meter increments

    // === Flatten terrain ===
    //grid.flattenAABB(i32AABB2(bp.bottomLeftWorldPos.x, bp.bottomLeftWorldPos.y, bp.dims.x, bp.dims.y), meanHeight);
    TileContainer& tileContainer = *building.getTileContainer();

    std::vector<Tile>& tiles = tileContainer.mTiles;
    std::vector<TileContainer*> dirtyNavTileContainers;
    const i32v3 dims(aabb.dims.x, aabb.dims.y, bp.floorCount);
    const i32 floorStride = dims.x * dims.y;

    // === Set world tiles, flatten heightmap, and track occupied bits ===
    IHeightmapGrid& grid = mWorld.getHeightmapGrid();
    for (ui32 i = 0; i < bp.tileTargetCount; ++i) {
        BuildingBlueprintTileTarget& tileTarget = bp.tileTargets[i];
        if (tileTarget.tileIndex < floorStride) {
            i32v2 tileWorldPos = worldPos + i32v2(tileTarget.tileIndex % dims.x, tileTarget.tileIndex / dims.x);
            // Epsilon to prevent z fighting
            grid.setHeightAtWorldPos(tileWorldPos, building.getTileAABB().z - 0.005f);
        }

        assert(isTileValid(tileTarget.id));
        Tile& tile = tiles[tileTarget.tileIndex];
        const TileDef& data = tileRepo.getLoadedOrUnloadedAsset(tileTarget.id);
        tile.layers[data.layer] = tileTarget.id;
        tile.setTileFlag(TileFlags::ROOFED);
        tileContainer.onTileChanged(tileTarget.tileIndex);
    }
    for (ui32 i = 0; i < bp.wallTargetCount; ++i) {
        BuildingBlueprintWallTarget& wallTarget = bp.wallTargets[i];
        assert(isTileValid(wallTarget.id));
        TileWall newWall{ .wallID = wallTarget.id, .isDoor = false /*TODO: this is wrong...*/ };
        tileContainer.mTileWallsContainer.setWallAtTile(wallTarget.tileIndex, newWall, wallTarget.dir);
    }

    // Set stairs tiles
    for (i32 i = 0; i < bp.stairPieceCount; ++i) {
        StairPiece& stairPiece = bp.stairPieces[i];
        const f32v3 tilePos = tileContainer.getTileSpatialGrid().getTileXYZOffsetWithZScale(stairPiece.pos);
        // Place stair steps
        const f32 heightAdd = stairPiece.height * STAIR_TILE_HEIGHT;
        const f32 stairPieceBaseHeight = tilePos.z + heightAdd;
        Tile& tile = tiles[stairPiece.pos];
        tile.groundLayer = bp.defaultFloorID;
        tile.mainLayer = stairPiece.isFlatPart ? bp.stairsFlatTileID : bp.stairsTileID;
        tile.setGroundZOffset(tilePos.z + heightAdd);
        tile.setOrientation(stairPiece.dir, TileLayer::Main);
        tile.setTileFlag(TileFlags::ROOFED);
        // Mark above tile as roofed as well
        tiles[stairPiece.pos + floorStride].setTileFlag(TileFlags::ROOFED);
        tileContainer.onTileChanged(stairPiece.pos);
    }

    TileContainerEvent loadFinishedEvent;
    loadFinishedEvent.container = &tileContainer;
    tileContainer.getWorld().getTileContainerRepository().dispatchLoadFinished(loadFinishedEvent);

    //building.mFunction = bp.desc->function;
    //building.mDoorTiles = blueprint.exteriorDoors;
    //assert(building.mDoorTiles.size());
    //assert(building.mRooms.size());

    // Mark ready for access
    building.getTileContainer()->setState(TileContainerState::READY);

    // Navmesh dirty
    if (NavWorld* navWorld = mWorld.tryGetNavWorld()) {
        navWorld->markContainerNavDirty(building.getTileContainer());
    }
}

void TileContainerLoader::loadChunkFromSimChunk(TileContainer& container) const
{
    // TODO: Go from SimulatedChunk somehow (SimulatedChunk vs SimulatedStructure)
    Chunk* chunk = container.getOwnerChunk();
    assert(chunk);

    // No need to incref, we cannot be destroyed while activating

    Services::Threadpool::ref().addTask([this, chunk]() {
        TileContainer& container = *chunk->mTileContainer;
        // Worker thread
        //
        // Initialize containers lookupap
        chunk->mTileContainersLookup = std::make_unique<ChunkTileContainersLookup>();
        chunk->mTileContainersLookup->chunkContainerID = container.getId();
        for (int i = 0; i < CHUNK_SIZE; ++i) {
            chunk->mTileContainersLookup->structureContainers[i] = INVALID_TILE_CONTAINER_ID;
        }

        // Generate chunk
        chunk->getWorld().getWorldGenerator().generateChunkFromSimChunk(*chunk);

        // Build visibility
        container.mTileVisibilityContainer.init(&container.getTileSpatialGrid(), container.getTiles(), container.getTileWallContainer());

        chunk->setState(ChunkState::WAITING_BUILDINGS);
    });
}

void TileContainerLoader::initEvents() {
    IChunkGrid& chunkGrid = mWorld.getChunkGrid();
    chunkGrid.registerChunkGridListeners(mChunkGridListeners);
    chunkGrid.addBeginLoadListener(mChunkGridListeners, [this](ChunkGridEvent& evnt) {
        ASSERT_GAME_THREAD();
        loadChunkFromSimChunk(*evnt.chunk.getTileContainer());
    });
}
