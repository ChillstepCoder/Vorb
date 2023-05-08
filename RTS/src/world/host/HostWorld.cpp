#include "stdafx.h"
#include "HostWorld.h"

#include "world/srv/SrvChunkGrid.h"
#include "world/srv/SrvHeightmapGrid.h"

#include "tile/TileContainerRepository.h"

#include "ecs/IEntityComponentSystem.h"
#include "ecs/srv/SrvEntityComponentSystem.h"
#include "physics/PhysicsWorld.h"
#include "pathfinding/NavThread.h"

#include "generation/WorldGenerationData.h"

HostWorld::HostWorld(ui32 widthTiles, IChunkGrid* chunkGrid, IHeightmapGrid* heightmapGrid) : IWorld(widthTiles, chunkGrid, heightmapGrid)
{
    mEcs = std::make_unique<SrvEntityComponentSystem>(*this);
}

void HostWorld::init() {

    mChunkGrid->setWorldAndAllocateChunks(*this);
    mHeightmapGrid->setWorld(*this);

    initClient(*this);
    initSrv(*this);
}

void HostWorld::tick(f32 elapsedSec) {
    PROFILE_FUNCTION();

    assert(mEcs);
    ASSERT_GAME_THREAD();

    // Update services
    // TODO: Could we use remaining frame time for these?
    Services::Threadpool::ref().mainThreadUpdate();
    Services::NavThread::ref().mainThreadUpdate();

    // TODO: REMOVE Update any pending updates if pathfinding is idle
  /*  for (auto&& chunk : getActiveChunks()) {
        chunk->updateMainThread();
    }*/

    // Update all dynamic tiles
    // TODO: Handle a different way?
    auto&& tileContainers = mTileContainerRepository->getTileContainers();
    for (auto&& it : tileContainers) {
        it.second->updateActiveDynamicTiles();
    }

    // TODO: Figure out best order
    tickShared(elapsedSec);
    tickSrv();

    //if (sWorldGen.mIsDirty) {
    //    sWorldGen.mIsDirty = false;
    //    std::cout << "NEED TO IMPLEMENT WORLD GENERATION EDITOR REFRESH\n";
    //   // debugRefreshWorldGeneration();
    //}

    // TickClient always last as it updates render state
    tickClient(*this);
}

void HostWorld::onFrameBegin() {
    ASSERT_RENDER_THREAD();

}

void HostWorld::frameUpdate(const Camera3D& camera, f32 elapsedSec) {
    ASSERT_RENDER_THREAD();
}

void HostWorld::onWorldBegin(const f32v2& loadCenter) {
    PROFILE_FUNCTION();

    onWorldBeginShared(loadCenter);
    onWorldBeginClient(*this);

    // TODO: Move
    mEcs->setLocalPlayer(mEcs->createEntity(getDefaultSpawn(), StrToken("player"), true));
}

void HostWorld::dirtyGrassFromBrush(const f32v2& pos, f32 brushRadius) {
    cliDirtyGrassFromBrush(pos, brushRadius);
}

WorldNetMode HostWorld::getNetMode()
{
    return WorldNetMode::Host;
}

WorldType HostWorld::getWorldType()
{
    return WorldType::Game;
}
