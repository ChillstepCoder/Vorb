#include "stdafx.h"
#include "ServerChunkAuthorityManager.h"

#include "server/ServerReportStateManager.h"

bool isChunkInLoadRange(const f32v2 worldPos, const f32v2 loadCenter, f32 loadRangeSq) {
    const f32v2 centerPos = worldPos + f32v2(HALF_CHUNK_WIDTH);
    const f32 distSq = glm::length2(centerPos - loadCenter);
    return distSq <= loadRangeSq;
}

ServerChunkAuthorityManager::ServerChunkAuthorityManager(i32 worldWidthChunks, ServerReportStateManager& playerMgr) :
    mWorldWidthChunks(worldWidthChunks),
    mReportStateManager(playerMgr)
{

    playerMgr.registerServerReportStateManagerListeners(mPlayerManagerListeners);
    playerMgr.addPlayerRegisteredListener([this](const ServerPlayerEvent& event) {
        ASSERT_SERVER_THREAD();
    });
    playerMgr.addPlayerUnregisteredListener([this](const ServerPlayerEvent& event) {
        ASSERT_SERVER_THREAD();
        PlayerActiveChunksData& playerData = mPlayerData[event.playerId];
        for (ChunkID chunkId : playerData.authoritativeChunkIds) {
            tryAssignNewChunkAuthority(chunkId);
        }
        playerData.authoritativeChunkIds.clear();
    });
    playerMgr.addPlayerReportChangedListener([this](const ServerPlayerReportChangedEvent& evnt) {
        ASSERT_SERVER_THREAD();
        for (ChunkID chunkId : evnt.removedReportChunks) {
            auto it = mChunkAuthorityPlayers.find(chunkId);
            if (mChunkAuthorityPlayers.find(chunkId) != mChunkAuthorityPlayers.end() && it->second == evnt.playerId) {
                tryAssignNewChunkAuthority(chunkId);
            }
        }
    });
    playerMgr.addChunkActiveChangedListener([this](const ServerPlayerChunkActiveChangeEvent& event) {
        ASSERT_SERVER_THREAD();
        PlayerActiveChunksData& playerData = mPlayerData[event.playerId];
        if (event.isActive) {
            playerData.authoritativeChunkIds.insert(event.chunkId);
            mChunkAuthorityPlayers[event.chunkId] = event.playerId;
            ServerChunkStateEvent evnt(event.chunkId, event.playerId);
            dispatchPlayerGainAuthority(evnt);
        } else {
            auto it = mChunkAuthorityPlayers.find(event.chunkId);
            mPlayerData[it->second].authoritativeChunkIds.erase(event.chunkId);
            mChunkAuthorityPlayers.erase(it);
            ServerChunkStateEvent evnt(event.chunkId, event.playerId);
            dispatchPlayerLoseAuthority(evnt);
        }
    });
}

void ServerChunkAuthorityManager::tick(f64 dt) {
    ASSERT_SERVER_THREAD();
}

bool ServerChunkAuthorityManager::isChunkActive(ChunkID chunkId) const {
    return mReportStateManager.isChunkActive(chunkId);
}

void ServerChunkAuthorityManager::tryAssignNewChunkAuthority(ChunkID chunkId) {
    ServerPlayerID newAuthority = mReportStateManager.getFirstReportingPlayerToChunk(chunkId);
    if (chunkId != INVALID_CHUNK_ID) {
        mPlayerData[newAuthority].authoritativeChunkIds.insert(chunkId);
        mChunkAuthorityPlayers[chunkId] = newAuthority;
        ServerChunkStateEvent evnt(chunkId, newAuthority);
        dispatchPlayerGainAuthority(evnt);
    }
    else {
        mChunkAuthorityPlayers.erase(chunkId);
    }
}
