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

void TileContainerLoader::loadChunk(TileContainer& container)
{
    // TODO: Go from SimulatedChunk somehow (SimulatedChunk vs SimulatedStructure)
    Chunk* chunk = container.getOwnerChunk();
    assert(chunk);
    chunk->incRef();

    Services::Threadpool::ref().addTask([chunk]() {
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
        chunk->getWorld().getWorldGenerator().generateChunk(*chunk);

        // Build visibility
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
