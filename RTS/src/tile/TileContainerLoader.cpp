#include "stdafx.h"
#include "TileContainerLoader.h"

#include "world/World.h"
#include "world/IHeightmapGrid.h"
#include "world/Chunk.h"
#include "world/ecosystem/FishEcosystem.h"
#include "pathfinding/NavWorld.h"
#include "generation/IWorldGenerator.h"

#include "visibility/VisibilityManager.h"

#include "tile/TileContainer.h"
#include "tile/TileContainerRepository.h"

#include "tile/TileContainerLoader.h"

TileContainerLoader::TileContainerLoader(World& world) : mWorld(world) {}

void TileContainerLoader::loadTerrainTileContainer(TileContainer& container)
{

    // TODO: Go from SimulatedChunk somehow (SimulatedChunk vs SimulatedStructure)

    Chunk* chunk = container.getOwnerChunk();
    assert(chunk);
    chunk->incRef();
    // Make sure we dont lose height data
    // TODO: copy minimum
    f32* heightData = new f32[HEIGHTMAP_VERT_SIZE_PER_PATCH];
    IHeightmapGrid& heightGrid = mWorld.getHeightmapGrid();
    const f32* srcData = heightGrid.getHeightDataAt(chunk->getHeightmapPatchID())->data;
    memcpy(heightData, srcData, sizeof(f32) * HEIGHTMAP_VERT_SIZE_PER_PATCH);

    Services::Threadpool::ref().addTask([chunk, heightData](ThreadPoolWorkerData* workerData) {
        // Worker thread

        // Generate chunk
        chunk->getWorld().getWorldGenerator().generateChunk(*chunk, heightData);
        delete heightData;

        // Build visibility
        TileContainer& container = *chunk->mTileContainer;
        container.mTileVisibilityContainer.init(&container.getTileSpatialGrid(), container.getTiles(), container.getTileWallContainer());

    }, [chunk, this]() {
        // Game thread
        
        // Ecosystem
        chunk->getWorld().getFishEcosystem().initChunkFish(*chunk);

        chunk->setState(ChunkState::LOADING_MESH_PHYSICS_NAV_VISIBILITY);

        // Cache harvestables
        chunk->mTileContainer->mHarvestableRegistry.refreshFromOwner();

        // Begin nav load
        if (NavWorld* navWorld = mWorld.tryGetNavWorld()) {
            navWorld->markContainerNavDirty(chunk->mTileContainer);
        }

        // Begin vis load
        mWorld.getVisibilityManager().initContainerVisibility(*chunk->mTileContainer);

        // Tile container loaded
        chunk->mTileContainer->setState(TileContainerState::READY);

        // Dispatch load finished
        TileContainerEvent loadFinishedEvent;
        loadFinishedEvent.container = chunk->mTileContainer;
        mWorld.getTileContainerRepository().dispatchLoadFinished(loadFinishedEvent);

        chunk->decRef();
    });
}
