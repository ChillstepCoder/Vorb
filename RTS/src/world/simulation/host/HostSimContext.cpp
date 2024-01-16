#include "stdafx.h"
#include "HostSimContext.h"

#include "world/simulation/host/SimThread.h"
#include "world/World.h"
#include "world/IChunkGrid.h"

#include "world/simulation/host/component/SimComponents.h"
#include "world/simulation/host/component/SettlementComponents.h"
#include "world/simulation/host/StoryTeller.h"

HostSimContext::HostSimContext(World& world) :
    WorldContextObject(world),
    mChunkData(world.getChunkGrid().getTotalChunks()),
    mTotalChunks(world.getChunkGrid().getTotalChunks())
{

    mStoryTeller = std::make_unique<StoryTeller>();
    mChunkStates.resizeAndZero(mTotalChunks);
    
    mSimThread = std::make_unique<SimThread>(*this, world);

}

HostSimContext::~HostSimContext()
{

}
