#include "stdafx.h"
#include "CliWorld.h"

#include "world/cli/CliChunkGrid.h"
#include "world/cli/CliHeightmapGrid.h"

#include "ecs/cli/CliEntityComponentSystem.h"

#include "physics/PhysicsWorld.h"

#include "resources/AssetLoader.h"

CliWorld::CliWorld(ui32 widthTiles, IChunkGrid* chunkGrid, IHeightmapGrid* heightmapGrid, WorldGeneratorType generatorType) : IWorld(widthTiles, chunkGrid, heightmapGrid, generatorType) {
    mEcs = std::make_unique<CliEntityComponentSystem>(*this);
}

void CliWorld::init() {
    mChunkGrid->setWorldAndAllocateChunks(*this);
    mHeightmapGrid->setWorld(*this);

    initClient(*this);
}

void CliWorld::tick(f32 elapsedSec) {
    ASSERT_GAME_THREAD();

    // Update services
    Services::Threadpool::ref().mainThreadUpdate();

    // Update pending assets
    AssetLoader::getInstance().update();

    // Update any pending updates if pathfinding is idle
    // TODO: REMOVE
    /*for (auto&& cid : getActiveChunks()) {
        mChunkGrid->getChunk(cid).updateMainThread();
    }*/

    mPhysWorld->stepSimulation(elapsedSec);

    // TODO: Figure out best order
    tickShared(elapsedSec);

    // TickClient always last as it updates render state
    tickClient(*this);
}

void CliWorld::onFrameBegin() {
    ASSERT_RENDER_THREAD();

}

void CliWorld::frameUpdate(const Camera3D& camera, f32 elapsedSec)
{
    ASSERT_RENDER_THREAD();
    // Physworld will handle internal interpolation and timestep itself
    //mPhysWorld->stepSimulation(elapsedSec);
}

void CliWorld::onWorldBegin(const f32v2& loadCenter) {
    onWorldBeginShared(loadCenter);
    onWorldBeginClient(*this);
}


WorldNetMode CliWorld::getNetMode() {
    return WorldNetMode::Client;
}

WorldType CliWorld::getWorldType() {
    return WorldType::Game;
}
