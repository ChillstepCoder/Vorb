#include "stdafx.h"
#include "GameServerNew.h"

#include "server/ServerReportStateManager.h"
#include "world/srv/ServerChunkAuthorityManager.h"

#include "options/DebugOptions.h"

#include "world/LocalChunkGrid.h"

#include "world/World.h"

GameServerNew::GameServerNew(World& world) : mWorld(world) {
    SERVER_THREAD_ID = std::this_thread::get_id();
    mPlayerManager = std::make_unique<ServerReportStateManager>(mWorld.getWidthChunks());
    mAuthorityManager = std::make_unique<ServerChunkAuthorityManager>(mWorld.getWidthChunks(), *mPlayerManager);

    mAuthorityManager->registerServerChunkAuthorityManagerListeners(mAuthorityManagerListeners);
    mAuthorityManager->addPlayerGainAuthorityListener(mAuthorityManagerListeners, [this](const ServerChunkStateEvent& evnt) {
        ASSERT_SERVER_THREAD();
        if (evnt.playerId == mLocalPlayerId) {
            LocalChunkGrid& chunkGrid = mWorld.getLocalChunkGrid();
            chunkGrid.setChunkActive(evnt.chunkId);

            // TODO: Dispatch new authority to each player?
        }
        else {
            // Dispatch message
            assert(false);
        }
    });
    mAuthorityManager->addPlayerLoseAuthorityListener(mAuthorityManagerListeners, [this](const ServerChunkStateEvent& evnt) {
        ASSERT_SERVER_THREAD();
        if (evnt.playerId == mLocalPlayerId) {
            LocalChunkGrid& chunkGrid = mWorld.getLocalChunkGrid();
            chunkGrid.setChunkInactive(evnt.chunkId);

            // TODO: Dispatch new authority to each player?
        }
        else {
            // Dispatch message
            assert(false);
        }
    });
}

GameServerNew::~GameServerNew() = default;

void GameServerNew::tick(f64 deltaTime) {
    PROFILE_FUNCTION();
    ASSERT_SERVER_THREAD();
    mPlayerManager->tick();
    mAuthorityManager->tick(deltaTime);
}

ServerPlayerID GameServerNew::initLocalPlayer(f32v3 position) {
    assert(mPlayerManager->hasHostPlayer() == false);
    mLocalPlayerId = mPlayerManager->registerPlayer(position, sDebugOptions.mLoadRange / CHUNK_WIDTH);
    return mLocalPlayerId;
}

void GameServerNew::setLocalPlayerPosition(f32v3 position) {
    mPlayerManager->setLocalPlayerPosition(position);
}
