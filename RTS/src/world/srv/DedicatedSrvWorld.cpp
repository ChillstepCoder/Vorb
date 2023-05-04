#include "stdafx.h"
#include "DedicatedSrvWorld.h"

#include "world/srv/SrvChunkGrid.h"
#include "world/srv/SrvHeightmapGrid.h"

void DedicatedSrvWorld::onWorldBegin(const f32v2& loadCenter)
{
    throw std::logic_error("The method or operation is not implemented.");
}

void DedicatedSrvWorld::dirtyGrassFromBrush(const f32v2& pos, f32 brushRadius)
{
    UNUSED(pos);
    UNUSED(brushRadius);
    // Do nothing
}

void DedicatedSrvWorld::tick(f32 elapsedSec)
{
    throw std::logic_error("The method or operation is not implemented.");
}

WorldNetMode DedicatedSrvWorld::getNetMode()
{
    return WorldNetMode::DedicatedServer;
}

WorldType DedicatedSrvWorld::getWorldType()
{
    return WorldType::Game;
}
