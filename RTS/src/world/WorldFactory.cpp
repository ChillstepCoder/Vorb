#include "stdafx.h"
#include "WorldFactory.h"

#include "world/host/HostWorld.h"
#include "world/srv/DedicatedSrvWorld.h"
#include "world/srv/SrvChunkGrid.h"
#include "world/srv/SrvHeightmapGrid.h"
#include "world/cli/CliWorld.h"
#include "world/cli/CliChunkGrid.h"
#include "world/cli/CliHeightmapGrid.h"

std::unique_ptr<IWorld> WorldFactory::makeWorld(WorldNetMode type, ui32 worldWidthTiles) {
    assert(worldWidthTiles < MAX_WORLD_WIDTH_TILES);

    // Clamp world width to multiple of HEIGHTMAP_WIDTH
    worldWidthTiles = (worldWidthTiles / HEIGHTMAP_WIDTH) * HEIGHTMAP_WIDTH;
    if (worldWidthTiles == 0) {
        worldWidthTiles = HEIGHTMAP_WIDTH;
    }

    switch (type) {
        case WorldNetMode::Client:
            return makeClientWorld(worldWidthTiles);
            break;
        case WorldNetMode::Editor:
            return makeEditorWorld(worldWidthTiles);
            break;
        case WorldNetMode::Host:
            return makeHostWorld(worldWidthTiles);
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

std::unique_ptr<CliWorld> WorldFactory::makeClientWorld(ui32 worldWidthTiles) {
    CliHeightmapGrid* heightmapGrid = new CliHeightmapGrid(worldWidthTiles);
    CliChunkGrid* chunkGrid = new CliChunkGrid();
    std::unique_ptr<CliWorld> newWorld = std::make_unique<CliWorld>(worldWidthTiles, chunkGrid, heightmapGrid);

    // World references
    chunkGrid->setWorldAndAllocateChunks(*newWorld);
    heightmapGrid->mWorld = newWorld.get();

    return newWorld;
}

std::unique_ptr<CliWorld> WorldFactory::makeEditorWorld(ui32 worldWidthTiles) {
    CliHeightmapGrid* heightmapGrid = new CliHeightmapGrid(worldWidthTiles);
    CliChunkGrid* chunkGrid = new CliChunkGrid();
    std::unique_ptr<CliWorld> newWorld = std::make_unique<CliWorld>(worldWidthTiles, chunkGrid, heightmapGrid);

    // World references
    chunkGrid->setWorldAndAllocateChunks(*newWorld);
    heightmapGrid->mWorld = newWorld.get();

    return newWorld;
}

std::unique_ptr<HostWorld> WorldFactory::makeHostWorld(ui32 worldWidthTiles) {
    SrvHeightmapGrid* heightmapGrid = new SrvHeightmapGrid(worldWidthTiles);
    SrvChunkGrid* chunkGrid = new SrvChunkGrid();
    std::unique_ptr<HostWorld> newWorld = std::make_unique<HostWorld>(worldWidthTiles, chunkGrid, heightmapGrid);

    // World references
    chunkGrid->setWorldAndAllocateChunks(*newWorld);
    heightmapGrid->mWorld = newWorld.get();

    return newWorld;
}

std::unique_ptr<DedicatedSrvWorld> WorldFactory::makeServerWorld(ui32 worldWidthTiles) {
    SrvHeightmapGrid* heightmapGrid = new SrvHeightmapGrid(worldWidthTiles);
    SrvChunkGrid* chunkGrid = new SrvChunkGrid();
    std::unique_ptr<DedicatedSrvWorld> newWorld = std::make_unique<DedicatedSrvWorld>(worldWidthTiles, chunkGrid, heightmapGrid);

    // World references
    chunkGrid->setWorldAndAllocateChunks(*newWorld);
    heightmapGrid->mWorld = newWorld.get();

    return newWorld;
}
