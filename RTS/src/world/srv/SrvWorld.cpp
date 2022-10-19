#include "stdafx.h"
#include "SrvWorld.h"

#include "world/srv/SrvChunkGrid.h"
#include "world/srv/SrvHeightmapGrid.h"

void SrvWorld::onWorldBegin(const f32v2& loadCenter)
{
    throw std::logic_error("The method or operation is not implemented.");
}

void SrvWorld::dirtyTerrainFromBrush(const f32v2& pos, f32 brushRadius)
{
    sharedDirtyTerrainFromBrush(pos, brushRadius);
}
