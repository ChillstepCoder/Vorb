#include "stdafx.h"
#include "CliChunkGrid.h"

#include "world/Chunk.h"
#include "world/World.h"

#include "tile/TileContainerRepository.h"

// TODO: Reduce copy paste
void CliChunkGrid::updateLoadingChunks() {
    assert(false); // Fix this, use IChunkGrid and better interface
    IHeightmapGrid& heightGrid = mWorld->getHeightmapGrid();
    for (size_t i = 0; i < mLoadingChunks.size();) {
        Chunk& chunk = mChunks[mLoadingChunks[i]];
        TileContainer* container = chunk.getTileContainer();
        switch (chunk.getState()) {
            case ChunkState::WAITING_HEIGHT: {
                // Poll for generated height
                if (heightGrid.tryGetHeightDataAt(chunk.getHeightmapPatchID())) {
                    beginTileLoadForChunk(chunk);
                }
                ++i;
                break;
            }
            case ChunkState::LOADING_TILES: {
                ++i;
                break;
            }
            case ChunkState::TILE_LOAD_FINISHED: {
                chunk.setState(ChunkState::WAITING_MESH_PHYSICS_NAV_VISIBILITY);
                container->setState(TileContainerState::WAITING_MESH_PHYSICS_VISIBILITY);

                // Cache harvestables
                container->getHarvestableRegistry().refreshFromOwner();

                TileContainerEvent loadFinishedEvent;
                loadFinishedEvent.container = chunk.mTileContainer;
                mWorld->getTileContainerRepository().dispatchLoadFinished(loadFinishedEvent);
                ++i;
                break;
            }
            case ChunkState::WAITING_MESH_PHYSICS_NAV_VISIBILITY: {
                if (container->didInitMeshPhysics()) {
                    mLoadingChunks[i] = mLoadingChunks.back();
                    mLoadingChunks.pop_back();
                    chunk.mFlags.clearBit(ChunkFlags::IN_LOAD_LIST);
                    onChunkReady(chunk);
                }
                else {
                    ++i;
                }
                break;
            }
            default:
                assert(false);
        }
    }
}
