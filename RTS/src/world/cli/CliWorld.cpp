#include "stdafx.h"
#include "CliWorld.h"

#include "world/cli/CliChunkGrid.h"
#include "world/cli/CliHeightmapGrid.h"

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
    throw std::logic_error("The method or operation is not implemented.");
}

void CliWorld::frameUpdate(const Camera3D& camera, f32 elapsedSec)
{
    throw std::logic_error("The method or operation is not implemented.");
}

void CliWorld::onWorldBegin(const f32v2& loadCenter) {
    onWorldBeginShared(loadCenter);
    onWorldBeginClient();
}
