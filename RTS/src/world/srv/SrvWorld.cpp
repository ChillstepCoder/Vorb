#include "stdafx.h"
#include "SrvWorld.h"

#include "world/srv/SrvChunkGrid.h"
#include "world/srv/SrvHeightmapGrid.h"

SrvWorld::SrvWorld() : IWorld(std::make_unique<IChunkGrid>(new SrvChunkGrid), std::make_unique<IHeightmapGrid>(new SrvHeightmapGrid))
{

}
