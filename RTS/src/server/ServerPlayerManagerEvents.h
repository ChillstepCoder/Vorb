#pragma once

#include <Vorb/Event.hpp>

class ServerReportStateManager;

enum class ServerPlayerEventType {
    PlayerRegistered,
    PlayerUnregistered,
    PlayerLoadRangeChanged,
    PlayerReportChanged,
    ChunkActiveChanged
};

struct ServerPlayerEvent {
    ServerPlayerID playerId;
    std::variant<i32, std::monostate> data;
};

struct ServerPlayerReportChangedEvent : public ServerPlayerEvent {
    ServerPlayerReportChangedEvent(
        ServerPlayerID playerId, const std::vector<ChunkID>& newReportChunks, const std::unordered_set<ChunkID>& removedReportChunks
    ) :
        ServerPlayerEvent{ playerId, std::monostate{} },
        newReportChunks(newReportChunks),
        removedReportChunks(removedReportChunks) {
    }

    const std::vector<ChunkID>& newReportChunks;
    const std::unordered_set<ChunkID>& removedReportChunks;
};

struct ServerPlayerChunkActiveChangeEvent : public ServerPlayerEvent {
    ServerPlayerChunkActiveChangeEvent(
        ServerPlayerID playerId, ChunkID chunkId, bool isActive
    ) :
        ServerPlayerEvent{ playerId, std::monostate{} },
        chunkId(chunkId),
        isActive(isActive) {
    }

    ChunkID chunkId;
    bool isActive;
};

EVENT_DISPATCHER_TYPE(ServerReportStateManager, ServerPlayerEventType, const ServerPlayerEvent&);