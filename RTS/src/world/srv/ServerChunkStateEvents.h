#pragma once

#include <Vorb/Event.hpp>

class ServerChunkAuthorityManager;

enum class ServerChunkStateEventType {
    PlayerGainAuthority,
    PlayerLoseAuthority,
};

struct ServerChunkStateEvent {
    ServerChunkStateEvent(ChunkID chunkId, ServerPlayerID playerId) :
        chunkId(chunkId),
        playerId(playerId) {
    }

    ChunkID chunkId;
    ServerPlayerID playerId;
};

EVENT_DISPATCHER_TYPE(ServerChunkAuthorityManager, ServerChunkStateEventType, const ServerChunkStateEvent&);