#include "stdafx.h"
#include "HostSimContext.h"

#include "world/simulation/host/SimThread.h"
#include "world/World.h"
#include "world/IChunkGrid.h"

#include "world/simulation/host/component/SimComponents.h"
#include "world/simulation/host/component/SettlementComponents.h"

HostSimContext::HostSimContext(World& world) :
    WorldContextObject(world),
    mChunkData(world.getChunkGrid().getTotalChunks()),
    mTotalChunks(world.getChunkGrid().getTotalChunks())
{
    
    mSimThread = std::make_unique<SimThread>(*this, world);
    mChunkStates.resizeAndZero(mTotalChunks);
}

HostSimContext::~HostSimContext()
{

}
