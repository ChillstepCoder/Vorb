#include "stdafx.h"
#include "CliWorld.h"

#include "world/cli/CliChunkGrid.h"
#include "world/cli/CliHeightmapGrid.h"

#include "ecs/cli/CliEntityComponentSystem.h"

#include "physics/PhysicsWorld.h"

CliWorld::CliWorld(IChunkGrid* chunkGrid, IHeightmapGrid* heightmapGrid) : IWorld(chunkGrid, heightmapGrid)
{
    mEcs = std::make_unique<CliEntityComponentSystem>();
}

void CliWorld::tick(const f32v2& playerPos, f32 elapsedSec)
{
    // TODO: Figure out best order
    tickShared(playerPos, elapsedSec);
    tickClient();


    updateTimeOfDay();

    updateCities();

    updateParticleSystems(playerPos);

    updateClouds();
}

void CliWorld::onFrameBegin()
{
    // Update services
    Services::Threadpool::ref().mainThreadUpdate();

    // Update any pending updates if pathfinding is idle
    for (auto&& chunk : getActiveChunks()) {
        chunk->updateMainThread();
    }
}

void CliWorld::frameUpdate(const Camera3D& camera, f32 elapsedSec)
{
    mEcs->frameUpdate(camera);

    // Client only, rendering stuff
    updateChunkVisibility(camera, mChunkGrid->getActiveChunks());

    // Physworld will handle internal interpolation and timestep itself
    mPhysWorld->stepSimulation(elapsedSec);
}

void CliWorld::onWorldBegin(const f32v2& loadCenter) {
    onWorldBeginShared(loadCenter);
    onWorldBeginClient();
}
