#include "stdafx.h"
#include "GameServerNew.h"

#include "server/ServerReportStateManager.h"
#include "server/ServerThread.h"
#include "world/srv/ServerChunkAuthorityManager.h"

#include "options/DebugOptions.h"

#include "world/LocalChunkGrid.h"

#include "world/World.h"

#include "gamethread/GameThreadTasks.h"

GameServerNew::GameServerNew(World& world) : mWorld(world) {
    mPlayerManager = std::make_unique<ServerReportStateManager>(mWorld.getWidthChunks());
    mAuthorityManager = std::make_unique<ServerChunkAuthorityManager>(mWorld.getWidthChunks(), *mPlayerManager);

    mAuthorityManager->registerServerChunkAuthorityManagerListeners(mAuthorityManagerListeners);
    mAuthorityManager->addPlayerGainAuthorityListener(mAuthorityManagerListeners, [this](const ServerChunkStateEvent& evnt) {
        ASSERT_SERVER_THREAD();
        if (evnt.playerId == mLocalPlayerId) {
            GameThreadTasks::getInstance().addGenericTask([this, chunkId = evnt.chunkId]() {
                LocalChunkGrid& chunkGrid = mWorld.getLocalChunkGrid();
                chunkGrid.setChunkActive(chunkId);
            });
        }
        else {
            // Dispatch message
            assert(false);
        }
    });
    mAuthorityManager->addPlayerLoseAuthorityListener(mAuthorityManagerListeners, [this](const ServerChunkStateEvent& evnt) {
        ASSERT_SERVER_THREAD();
        if (evnt.playerId == mLocalPlayerId) {
            GameThreadTasks::getInstance().addGenericTask([this, chunkId = evnt.chunkId]() {
                LocalChunkGrid& chunkGrid = mWorld.getLocalChunkGrid();
                chunkGrid.setChunkInactive(chunkId);
            });

            // TODO: Dispatch new authority to each player?
        }
        else {
            // Dispatch message
            assert(false);
        }
    });

}

GameServerNew::~GameServerNew() = default;

void GameServerNew::start() {
    mServerThread = std::make_unique<ServerThread>(*this, mWorld);
    mServerThreadProducerToken = mServerThread->getNewProducerToken();
}

void GameServerNew::stop() {
    mServerThread.reset();
}

ServerPlayerID GameServerNew::initLocalPlayer(f32v3 position) {
    ASSERT_GAME_THREAD();

    assert(mPlayerManager->hasHostPlayer() == false);
    mServerThread->addTask(*mServerThreadProducerToken, [this, position]() {
        const ServerPlayerID id = mPlayerManager->registerPlayer(position, sDebugOptions.mLoadRange / CHUNK_WIDTH);
        assert(id == 0);
    });

    // Always 0
    return ServerPlayerID(0);
}

void GameServerNew::setLocalPlayerPosition(f32v3 position) {
    ASSERT_GAME_THREAD();

    mServerThread->addTask(*mServerThreadProducerToken, [this, position]() {
        mPlayerManager->setLocalPlayerPosition(position);
    });
}
