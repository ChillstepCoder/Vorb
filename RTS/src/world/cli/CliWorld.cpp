#include "stdafx.h"
#include "CliWorld.h"

#include "world/cli/CliChunkGrid.h"
#include "world/cli/CliHeightmapGrid.h"

CliWorld::CliWorld() : IWorld(std::make_unique<IChunkGrid>(new CliChunkGrid), std::make_unique<IHeightmapGrid>(new CliHeightmapGrid))
{
}

void CliWorld::onFrameBegin()
{
    throw std::logic_error("The method or operation is not implemented.");
}

void CliWorld::frameUpdate(const Camera3D& camera, f32 elapsedSec)
{
    throw std::logic_error("The method or operation is not implemented.");
}

void CliWorld::initPostResourcesLoaded() {

    initPostResourcesLoadedClient();
}
