#include "stdafx.h"
#include "HostWorld.h"

#include "world/srv/SrvChunkGrid.h"
#include "world/srv/SrvHeightmapGrid.h"

#include "ecs/EntityComponentSystem.h"
#include "physics/PhysicsWorld.h"
#include "pathfinding/NavThread.h"

#include "generation/WorldGeneration.h"

HostWorld::HostWorld() : IWorld(std::make_unique<IChunkGrid>(new SrvChunkGrid), std::make_unique<IHeightmapGrid>(new SrvHeightmapGrid)) {

}

void HostWorld::tick(const f32v2& playerPos, f32 elapsedSec) {
    assert(mEcs);

    // TODO: Figure out best order
    tickShared(playerPos, elapsedSec);
    tickSrv();
    tickClient();

    if (sWorldGen.mIsDirty) {
        sWorldGen.mIsDirty = false;
        std::cout << "NEED TO IMPLEMENT WORLD GENERATION EDITOR REFRESH\n";
       // debugRefreshWorldGeneration();
    }

    updateTimeOfDay();

    updateCities();

    updateParticleSystems(playerPos);

    updateClouds();

}

void HostWorld::onFrameBegin() {

    // Update services
    Services::Threadpool::ref().mainThreadUpdate();
    Services::NavThread::ref().mainThreadUpdate();

    // Update any pending updates if pathfinding is idle
    if (!Services::NavThread::ref().isRunningPathfind()) {
        for (auto&& chunk : getActiveChunks()) {
            chunk->updateMainThread();
        }
    }

    // Update all dynamic tiles
    auto&& tileContainers = TileContainerRepository::getTileContainers();
    for (auto&& container : tileContainers) {
        container->updateActiveDynamicTiles();
    }
}

void HostWorld::frameUpdate(const Camera3D& camera, f32 elapsedSec) {

    mEcs->frameUpdate(camera);

    // Client only, rendering stuff
    updateChunkVisibility(camera, mChunkGrid->getActiveChunks());

    // Physworld will handle internal interpolation and timestep itself
    mPhysWorld->stepSimulation(elapsedSec);
}

void HostWorld::initPostResourcesLoaded() {

    initPostResourcesLoadedClient();
}
