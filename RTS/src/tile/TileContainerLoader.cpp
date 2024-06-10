#include "stdafx.h"
#include "TileContainerLoader.h"

#include "world/World.h"
#include "world/IHeightmapGrid.h"
#include "world/IChunkGrid.h"
#include "world/Chunk.h"
#include "world/ecosystem/FishEcosystem.h"
#include "building/building.h"
#include "building/BuildingBlueprint.h"
#include "building/BuildingGrid.h"
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

void TileContainerLoader::loadBuildingAsync(Building& building) const {
    building.setState(BuildingState::LOADING_TILES);
    if (building.getBlueprint()) {
        loadBuildingFromBlueprintAsync(building);
    }
    else {
        assert(false);
    }
}

// Allow limited access to private member
class TileContainerLoaderBuildingGridProxy {
public:
    static void onFinished(BuildingGrid& grid, Building& building) {
        grid.onBuildingFinishedLoad(building);
    }
};

void TileContainerLoader::loadBuildingFromBlueprintAsync(Building& building) const {
    PROFILE_FUNCTION();

    Services::Threadpool::ref().addTask([this, &building]() {

        TileRepository& tileRepo = TileRepository::get();
        BuildingBlueprint& bp = *building.getBlueprint();

        PreciseTimer timer;
        const i32v2 worldPos = bp.worldPosRootDTile.toTilePos();
        const i32AABB2 aabb(worldPos, bp.dimsDTile.toTilePos());

        // Clamp building height to 1 meter increments
        TileContainer& tileContainer = *building.getTileContainer();

        std::vector<Tile>& tiles = tileContainer.mTiles;
        const i32v3 dims(aabb.dims.x, aabb.dims.y, bp.floorCount);
        const i32 floorStride = dims.x * dims.y;

        // === Set world tiles, flatten heightmap, and track occupied bits ===
        IHeightmapGrid& grid = mWorld.getHeightmapGrid();
        for (ui32 i = 0; i < bp.tileTargetCount; ++i) {
            BuildingBlueprintTileTarget& tileTarget = bp.tileTargets[i];

            if (tileTarget.fillableRecipe.isConstructed()) {
                assert(isTileValid(tileTarget.id));
                Tile& tile = tiles[tileTarget.tileIndex];
                const TileDef& data = tileRepo.getLoadedOrUnloadedAsset(tileTarget.id);
                tile.layers[data.layer] = tileTarget.id;
                tile.tileFlags.setBit(TileFlags::ROOFED);
            }
        }

        for (ui32 i = 0; i < bp.wallTargetCount; ++i) {
            BuildingBlueprintWallTarget& wallTarget = bp.wallTargets[i];

            if (wallTarget.fillableRecipe.isConstructed()) {
                assert(isTileValid(wallTarget.id));
                TileWall newWall{ .wallID = wallTarget.id, .isDoor = wallTarget.isDoor };
                tileContainer.mTileWallsContainer.setWallAtTile(wallTarget.tileIndex, newWall, wallTarget.dir);
            }
        }

        // Set stairs tiles
        for (i32 i = 0; i < bp.stairTargetCount; ++i) {
            if (bp.stairTargets[i].fillableRecipe.isConstructed()) {
                StairPiece& stairPiece = bp.stairTargets[i].piece;
                const f32v3 tilePos = tileContainer.getTileSpatialGrid().getTileXYZOffsetWithZScale(stairPiece.pos);
                // Place stair steps
                const f32 heightAdd = stairPiece.height * STAIR_TILE_HEIGHT;
                const f32 stairPieceBaseHeight = tilePos.z + heightAdd;
                Tile& tile = tiles[stairPiece.pos];
                tile.groundLayer = bp.defaultFloorID;
                tile.mainLayer = stairPiece.isFlatPart ? bp.stairsFlatTileID : bp.stairsTileID;
                // TODO: Should we really be using tilePos.z here?
                tile.groundZOffset = tilePos.z + heightAdd;
                tile.setOrientation(stairPiece.dir, TileLayer::Main);
                tile.tileFlags.setBit(TileFlags::ROOFED);
                // Mark above tile as roofed as well
                tiles[stairPiece.pos + floorStride].tileFlags.setBit(TileFlags::ROOFED);
            }
        }

        // Set exterior flags
        for (TileIndex i = 0; i < tileContainer.getNumTiles(); ++i) {
            Tile& tile = tiles[i];
            // TODO: More robust checks
            if (!tile.isRoofed()) {
                tile.tileFlags.setBit(TileFlags::IS_BUILDING_EXTERIOR);
            }
        }

        //building.mFunction = bp.desc->function;
        //building.mDoorTiles = blueprint.exteriorDoors;
        //assert(building.mDoorTiles.size());
        //assert(building.mRooms.size());

        // Mark ready for access
        tileContainer.setDirtyData();
        tileContainer.setState(TileContainerState::READY);

        TileContainerLoaderBuildingGridProxy::onFinished(mWorld.getBuildingGrid(), building);

    });
}

void TileContainerLoader::loadChunkFromSimChunkAsync(TileContainer& container) const
{
    // TODO: Go from SimulatedChunk somehow (SimulatedChunk vs SimulatedStructure)
    Chunk* chunk = container.getOwnerChunk();
    assert(chunk);

    // Footprint must be copied
    BitArray buildingFootprint = mWorld.getBuildingGrid().getBuildingFootprintAtChunk(chunk->getChunkID());

    // No need to incref, we cannot be destroyed while activating
    Services::Threadpool::ref().addTask([this, chunk, buildingFootprint = std::move(buildingFootprint)]() {
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
        chunk->getWorld().getWorldGenerator().generateChunkFromSimChunk(*chunk, buildingFootprint);

        // Build visibility
        container.mTileVisibilityContainer.init(&container.getTileSpatialGrid(), container.getTiles(), container.getTileWallContainer());

        // Cache harvestables
        //container.mHarvestableRegistry.refreshFromOwner();

        chunk->setState(ChunkState::WAITING_BUILDINGS);
    });
}

void TileContainerLoader::initEvents() {
    IChunkGrid& chunkGrid = mWorld.getChunkGrid();
    chunkGrid.registerChunkGridListeners(mChunkGridListeners);
    chunkGrid.addBeginLoadListener(mChunkGridListeners, [this](ChunkGridEvent& evnt) {
        ASSERT_GAME_THREAD();
        loadChunkFromSimChunkAsync(*evnt.chunk.getTileContainer());
    });
}
