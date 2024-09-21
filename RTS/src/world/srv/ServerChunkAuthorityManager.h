#pragma once

#include "server/ServerConst.h"
#include "server/ServerPlayerManagerEvents.h"
#include "world/srv/ServerChunkStateEvents.h"

class ServerReportStateManager;

// Tracks chunks and assigns them to players
class ServerChunkAuthorityManager {
public:
    ServerChunkAuthorityManager(i32 worldWidthChunks, ServerReportStateManager& playerMgr);

    bool isChunkActive(ChunkID chunkId) const;

    EVENT_LISTENER_FUNCS(ServerChunkAuthorityManager, PlayerGainAuthority, ServerChunkStateEventType::PlayerGainAuthority, ServerChunkStateEvent&);
    EVENT_LISTENER_FUNCS(ServerChunkAuthorityManager, PlayerLoseAuthority, ServerChunkStateEventType::PlayerLoseAuthority, ServerChunkStateEvent&);
private:
    void tryAssignNewChunkAuthority(ChunkID chunkId);

    ServerReportStateManager& mReportStateManager;
    ServerReportStateManagerListeners mPlayerManagerListeners;

    struct PlayerActiveChunksData {
        // Chunks that this player is authoritative over in the distributed simulation
        std::unordered_set<ChunkID> authoritativeChunkIds;
    };

    i32 mWorldWidthChunks;
    std::unordered_map<ChunkID, ServerPlayerID> mChunkAuthorityPlayers;
    std::array<PlayerActiveChunksData, MAX_PLAYERS> mPlayerData;

    EVENT_DISPATCHER_DEF(ServerChunkAuthorityManager);
};

