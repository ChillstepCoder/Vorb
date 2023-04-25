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
    delete sHeightmapGrid;
    delete sChunkGrid;
    delete sMainGameWorld;
    sHeightmapGrid = nullptr;
    sChunkGrid = nullptr;
    sMainGameWorld = nullptr;
}

IWorld& WorldFactory::makeClientWorld() {
    sHeightmapGrid = new CliHeightmapGrid();
    sChunkGrid = new CliChunkGrid();
    sMainGameWorld = new CliWorld(sChunkGrid, sHeightmapGrid);
    return *sMainGameWorld;
}

IWorld& WorldFactory::makeHostWorld() {
    sHeightmapGrid = new SrvHeightmapGrid();
    sChunkGrid = new SrvChunkGrid();
    sMainGameWorld = new HostWorld(sChunkGrid, sHeightmapGrid);
    return *sMainGameWorld;
}

IWorld& WorldFactory::makeServerWorld() {
    sHeightmapGrid = new SrvHeightmapGrid();
    sChunkGrid = new SrvChunkGrid();
    sMainGameWorld = new DedicatedSrvWorld(sChunkGrid, sHeightmapGrid);
    return *sMainGameWorld;
}
