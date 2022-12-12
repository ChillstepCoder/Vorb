#include "stdafx.h"
#include "HostWorld.h"

#include "world/srv/SrvChunkGrid.h"
#include "world/srv/SrvHeightmapGrid.h"

#include "ecs/IEntityComponentSystem.h"
#include "ecs/srv/SrvEntityComponentSystem.h"
#include "physics/PhysicsWorld.h"
#include "pathfinding/NavThread.h"

#include "generation/WorldGeneration.h"

HostWorld::HostWorld(IChunkGrid* chunkGrid, IHeightmapGrid* heightmapGrid) : IWorld(chunkGrid, heightmapGrid)
{
    mEcs = std::make_unique<SrvEntityComponentSystem>();
}

void HostWorld::tick(f32 elapsedSec) {
    PROFILE_FUNCTION();

    assert(mEcs);
    assert(IS_GAME_THREAD());

    // Update services
    // TODO: Could we use remaining frame time for these?
    Services::Threadpool::ref().mainThreadUpdate();
    Services::NavThread::ref().mainThreadUpdate();

    // Update any pending updates if pathfinding is idle
    if (!Services::NavThread::ref().isRunningPathfind()) {
        PROFILE_SCOPE("Update chunks");
        for (auto&& chunk : getActiveChunks()) {
            chunk->updateMainThread();
        }
    }

    // Update all dynamic tiles
    auto&& tileContainers = TileContainerRepository::getTileContainers();
    for (auto&& container : tileContainers) {
        container->updateActiveDynamicTiles();
    }

    // TODO: Figure out best order
    tickShared(elapsedSec);
    tickSrv();

    //if (sWorldGen.mIsDirty) {
    //    sWorldGen.mIsDirty = false;
    //    std::cout << "NEED TO IMPLEMENT WORLD GENERATION EDITOR REFRESH\n";
    //   // debugRefreshWorldGeneration();
    //}

    updateTimeOfDay();

    updateCities();

    // TickClient always last as it updates render state
    tickClient(*this);
}

void HostWorld::onFrameBegin() {
    assert(IS_RENDER_THREAD());

}

void HostWorld::frameUpdate(const Camera3D& camera, f32 elapsedSec) {
    assert(IS_RENDER_THREAD());
}

void HostWorld::onWorldBegin(const f32v2& loadCenter) {
    PROFILE_FUNCTION();

    onWorldBeginShared(loadCenter);
    onWorldBeginClient();

    // TODO: Move
    mEcs->setLocalPlayer(mEcs->createEntity(WorldData::DEFAULT_PLAYER_SPAWN, StrToken("player"), true));
}

void HostWorld::dirtyTerrainFromBrush(const f32v2& pos, f32 brushRadius) {
    PROFILE_FUNCTION();
    cliDirtyTerrainFromBrush(pos, brushRadius);
    sharedDirtyTerrainFromBrush(pos, brushRadius);
}
