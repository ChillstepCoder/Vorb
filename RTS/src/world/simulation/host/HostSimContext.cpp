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

void HostSimContext::onWorldBeginGame() {

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
