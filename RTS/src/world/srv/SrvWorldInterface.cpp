#include "stdafx.h"
#include "SrvWorldInterface.h"

#include "pathfinding/NavWorld.h"

#include "item/ItemStockpileRegistry.h"
#include "world/ecosystem/FishEcosystem.h"

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
    mItemStockpileRegistry = std::make_unique<ItemStockpileRegistry>(world);
    mFishEcosystem = std::make_unique<FishEcosystem>(world);
}

void SrvWorldInterface::tickSrv(f32 elapsedSec) {
    mNavWorld->tickGameThread();
    mFishEcosystem->tickGameThread(elapsedSec);
}


