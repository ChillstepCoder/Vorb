#include "stdafx.h"
#include "SrvWorldInterface.h"

#include "pathfinding/NavWorld.h"

#include "item/ItemStockpileRegistry.h"

#include "city/City.h"
#include "pathfinding/NavThread.h"

SrvWorldInterface::SrvWorldInterface() {

}

SrvWorldInterface::~SrvWorldInterface()
{

}

void SrvWorldInterface::initSrv(IWorld& world) {
    mNavWorld = std::make_unique<NavWorld>(world);
    Services::NavThread::ref().init(*mNavWorld);
}

void SrvWorldInterface::tickSrv() {
    mNavWorld->tickGameThread();
}


