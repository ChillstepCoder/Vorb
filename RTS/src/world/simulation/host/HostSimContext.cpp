#include "stdafx.h"
#include "HostSimContext.h"

#include "world/simulation/host/SimThread.h"
#include "world/World.h"
#include "world/IChunkGrid.h"
#include "world/simulation/host/SimECS.h"

#include "world/simulation/host/component/SimComponents.h"
#include "world/simulation/host/component/SettlementComponents.h"
#include "world/simulation/host/StoryTeller.h"
#include "world/simulation/host/SimImmigrationManager.h"

HostSimContext::HostSimContext(World& world) :
    WorldContextObject(world),
    mChunkData(world.getChunkGrid().getTotalChunks()),
    mTotalChunks(world.getChunkGrid().getTotalChunks())
{
    mAnalytics = std::make_unique<SimWorldAnalytics>();
    mStoryTeller = std::make_unique<StoryTeller>();
    mChunkStates.resizeAndZero(mTotalChunks);
    mSimulatingChunks.resize(mTotalChunks);
    mSimulatingChunks.fill(true);
    mSimECS = std::make_unique<SimECS>(*this);
    mImmigrationManager = std::make_unique<SimImmigrationManager>(*this);
}

HostSimContext::~HostSimContext() {
    mSimThread.reset(); // Join
}

void HostSimContext::beginHistorySimulation() {

    mImmigrationManager->init();
    mAnalytics->setDesiredPopulation(10000);

    assert(!mSimThread);
    assert(!mSimulatingHistory);
    mSimThread = std::make_unique<SimThread>(*this, mWorld);
    mSimThread->setState(SimThreadState::HistorySim);
    mSimThread->setTargetTickRateMs(1.0);
    mSimThread->setTimeScale(100000.0f); // 100000x speed sim
    mSimThread->start();
    mSimulatingHistory = true;
}

void HostSimContext::endHistorySimulation() {
    // TODO: We should probably join here
    mSimThread->setState(SimThreadState::Idle);
}

void HostSimContext::onWorldBeginGame() {
    if (!mSimThread) {
        mSimThread = std::make_unique<SimThread>(*this, mWorld);
    }
    mSimThread->setTargetTickRateMs(200.0);
    mSimThread->setTimeScale(1.0f);
    mSimThread->setState(SimThreadState::GameSim);
}

void HostSimContext::registerPlayer(ServerPlayerID playerId, f32v3 startPos) {
    SimPlayer newPlayer;
    newPlayer.mPlayerId = playerId;
    newPlayer.mLastKnownPosition = startPos;
    mPlayers.push_back(newPlayer);
}

void HostSimContext::setPlayerPosition(ServerPlayerID playerId, f32v3 pos) {
    for (auto&& p : mPlayers) {
        if (p.mPlayerId == playerId) {
            p.mLastKnownPosition = pos;
            return;
        }
    }
}

void HostSimContext::removePlayer(ServerPlayerID playerId) {
    for (auto&& it = mPlayers.begin(); it != mPlayers.end(); ++it) {
        if (it->mPlayerId == playerId) {
            mPlayers.erase(it);
            return;
        }
    }
}

RandomGenerator& HostSimContext::getSimRandomGenerator() const {
    return mSimThread->getRandomGenerator();
}

ui32 HostSimContext::getWidthChunks() const {
    return mWorld.getChunkGrid().getWidthChunks();
}

void HostSimContext::debugRender(f32v3 cameraPos) const {
    mSimECS->debugRender(cameraPos);
}
