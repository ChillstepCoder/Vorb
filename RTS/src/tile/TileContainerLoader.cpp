#include "stdafx.h"
#include "TileContainerLoader.h"

#include "world/World.h"
#include "world/IHeightmapGrid.h"
#include "world/Chunk.h"

#include "tile/TileContainer.h"

#include "tile/TileContainerLoader.h"

TileContainerLoader::TileContainerLoader(World& world) : mWorld(world) {}

void TileContainerLoader::loadTerrainTileContainer(TileContainer& container)
{
    container.incRef();

    Chunk* owner = container.getOwnerChunk();
    assert(owner);
    // Make sure we dont lose height data
    // TODO: copy minimum
    f32* heightData = new f32[HEIGHTMAP_VERT_SIZE_PER_PATCH];
    IHeightmapGrid& heightGrid = mWorld.getHeightmapGrid();
    const f32* srcData = heightGrid.getHeightDataAt(owner->getHeightmapPatchID())->data;
    memcpy(heightData, srcData, sizeof(f32) * HEIGHTMAP_VERT_SIZE_PER_PATCH);

    Services::Threadpool::ref().addTask([owner, heightData](ThreadPoolWorkerData* workerData) {
        // Worker thread
        owner->getWorld().getWorldGenerator().generateChunk(owner, heightData);
        delete heightData;
        // Generate fish if needed
    }, [&chunk]() {
        // Game thread
        chunk.getWorld().getFishEcosystem().initChunkFish(chunk);
        chunk.setState(ChunkState::TILE_LOAD_FINISHED);
        chunk.decRef();
    });
}
