#include "stdafx.h"
#include "WorldFactory.h"

#include "world/host/HostWorld.h"
#include "world/srv/SrvWorld.h"
#include "world/srv/SrvChunkGrid.h"
#include "world/srv/SrvHeightmapGrid.h"
#include "world/cli/CliWorld.h"
#include "world/cli/CliChunkGrid.h"
#include "world/cli/CliHeightmapGrid.h"

IWorld& WorldFactory::makeWorld(WorldType type) {
    switch (type) {
        case WorldType::CLIENT:
            return makeClientWorld();
            break;
        case WorldType::HOST:
            return makeHostWorld();
            break;
        case WorldType::DEDICATED:
            assert(false);
            break;
        default:
            assert(false);
            break;

    }
    // Failure case
    assert(false);
    return *sWorld;
}

void WorldFactory::destroyWorld()
{
    delete sHeightmapGrid;
    delete sChunkGrid;
    delete sWorld;
    sHeightmapGrid = nullptr;
    sChunkGrid = nullptr;
    sWorld = nullptr;
}

IWorld& WorldFactory::makeClientWorld() {
    sHeightmapGrid = new CliHeightmapGrid();
    sChunkGrid = new CliChunkGrid();
    sWorld = new CliWorld(sChunkGrid, sHeightmapGrid);
    return *sWorld;
}

IWorld& WorldFactory::makeHostWorld() {
    sHeightmapGrid = new SrvHeightmapGrid();
    sChunkGrid = new SrvChunkGrid();
    sWorld = new HostWorld(sChunkGrid, sHeightmapGrid);
    return *sWorld;
}

IWorld& WorldFactory::makeServerWorld() {
    sHeightmapGrid = new SrvHeightmapGrid();
    sChunkGrid = new SrvChunkGrid();
    sWorld = new SrvWorld(sChunkGrid, sHeightmapGrid);
    return *sWorld;
}
