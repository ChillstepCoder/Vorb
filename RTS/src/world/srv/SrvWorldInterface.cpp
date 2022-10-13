#include "stdafx.h"
#include "SrvWorldInterface.h"

#include "pathfinding/NavWorld.h"

#include "item/ItemStockpileRegistry.h"

#include "city/City.h"
#include "pathfinding/NavThread.h"

SrvWorldInterface::SrvWorldInterface()
{
    // Nav graph
    mNavWorld = std::make_unique<NavWorld>();

    Services::NavThread::ref().init(*mNavWorld);
}

SrvWorldInterface::~SrvWorldInterface()
{

}

void SrvWorldInterface::tickSrv()
{

}


