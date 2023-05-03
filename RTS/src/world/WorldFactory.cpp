#include "stdafx.h"
#include "WorldFactory.h"

#include "world/host/HostWorld.h"
#include "world/srv/DedicatedSrvWorld.h"
#include "world/srv/SrvChunkGrid.h"
#include "world/srv/SrvHeightmapGrid.h"
#include "world/cli/CliWorld.h"
#include "world/cli/CliChunkGrid.h"
#include "world/cli/CliHeightmapGrid.h"

std::unique_ptr<IWorld> WorldFactory::makeWorld(WorldNetMode type) {
    switch (type) {
        case WorldNetMode::Client:
            return makeClientWorld();
            break;
        case WorldNetMode::Editor:
            return makeEditorWorld();
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
    static_assert(e_cast(WorldNetMode::COUNT) == 4);
    // Failure case
    throw std::invalid_argument("Invalid WorldNetMode value passed to makeWorld()");
}

void WorldFactory::destroyWorld()
{
    assert(false);
}

std::unique_ptr<CliWorld> WorldFactory::makeClientWorld() {
    CliHeightmapGrid* heightmapGrid = new CliHeightmapGrid();
    CliChunkGrid* chunkGrid = new CliChunkGrid(WorldData::WORLD_WIDTH_CHUNKS);
    std::unique_ptr<CliWorld> newWorld = std::make_unique<CliWorld>(chunkGrid, heightmapGrid);

    // World references
    chunkGrid->mWorld = newWorld.get();
    heightmapGrid->mWorld = newWorld.get();

    return newWorld;
}

std::unique_ptr<CliWorld> WorldFactory::makeEditorWorld() {
    CliHeightmapGrid* heightmapGrid = new CliHeightmapGrid();
    CliChunkGrid* chunkGrid = new CliChunkGrid(WorldData::WORLD_WIDTH_CHUNKS);
    std::unique_ptr<CliWorld> newWorld = std::make_unique<CliWorld>(chunkGrid, heightmapGrid);

    // World references
    chunkGrid->mWorld = newWorld.get();
    heightmapGrid->mWorld = newWorld.get();

    return newWorld;
}

std::unique_ptr<HostWorld> WorldFactory::makeHostWorld() {
    SrvHeightmapGrid* heightmapGrid = new SrvHeightmapGrid();
    SrvChunkGrid* chunkGrid = new SrvChunkGrid(WorldData::WORLD_WIDTH_CHUNKS);
    std::unique_ptr<HostWorld> newWorld = std::make_unique<HostWorld>(chunkGrid, heightmapGrid);

    // World references
    chunkGrid->mWorld = newWorld.get();
    heightmapGrid->mWorld = newWorld.get();

    return newWorld;
}

std::unique_ptr<DedicatedSrvWorld> WorldFactory::makeServerWorld() {
    SrvHeightmapGrid* heightmapGrid = new SrvHeightmapGrid();
    SrvChunkGrid* chunkGrid = new SrvChunkGrid(WorldData::WORLD_WIDTH_CHUNKS);
    std::unique_ptr<DedicatedSrvWorld> newWorld = std::make_unique<DedicatedSrvWorld>(chunkGrid, heightmapGrid);

    // World references
    chunkGrid->mWorld = newWorld.get();
    heightmapGrid->mWorld = newWorld.get();

    return newWorld;
}
