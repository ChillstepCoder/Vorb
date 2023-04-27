#include "stdafx.h"
#include "WorldFactory.h"

#include "world/host/HostWorld.h"
#include "world/srv/DedicatedSrvWorld.h"
#include "world/srv/SrvChunkGrid.h"
#include "world/srv/SrvHeightmapGrid.h"
#include "world/cli/CliWorld.h"
#include "world/cli/CliChunkGrid.h"
#include "world/cli/CliHeightmapGrid.h"

IWorld& WorldFactory::makeWorld(WorldNetMode type) {
    switch (type) {
        case WorldNetMode::Client:
            return makeClientWorld();
            break;
        case WorldNetMode::Host:
            return makeHostWorld();
            break;
        case WorldNetMode::DedicatedServer:
            assert(false);
            break;
        default:
            assert(false);
            break;

    }
    // Failure case
    assert(false);
    return *sMainGameWorld;
}

void WorldFactory::destroyWorld()
{
    assert(false);
}

IWorld& WorldFactory::makeClientWorld() {
    CliHeightmapGrid* heightmapGrid = new CliHeightmapGrid();
    CliChunkGrid* chunkGrid = new CliChunkGrid(WorldData::WORLD_WIDTH_CHUNKS);
    sMainGameWorld = new CliWorld(chunkGrid, heightmapGrid);

    // World references
    chunkGrid->mWorld = sMainGameWorld;
    heightmapGrid->mWorld = sMainGameWorld;

    return *sMainGameWorld;
}

IWorld& WorldFactory::makeHostWorld() {
    SrvHeightmapGrid* heightmapGrid = new SrvHeightmapGrid();
    SrvChunkGrid* chunkGrid = new SrvChunkGrid(WorldData::WORLD_WIDTH_CHUNKS);
    sMainGameWorld = new HostWorld(chunkGrid, heightmapGrid);

    // World references
    chunkGrid->mWorld = sMainGameWorld;
    heightmapGrid->mWorld = sMainGameWorld;

    return *sMainGameWorld;
}

IWorld& WorldFactory::makeServerWorld() {
    SrvHeightmapGrid* heightmapGrid = new SrvHeightmapGrid();
    SrvChunkGrid* chunkGrid = new SrvChunkGrid(WorldData::WORLD_WIDTH_CHUNKS);
    sMainGameWorld = new DedicatedSrvWorld(chunkGrid, heightmapGrid);

    // World references
    chunkGrid->mWorld = sMainGameWorld;
    heightmapGrid->mWorld = sMainGameWorld;

    return *sMainGameWorld;
}
