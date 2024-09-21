#pragma once
#include "ServerPlayerManagerEvents.h"
#include "ServerConst.h"


class ServerPlayerData {
public:
    f32v3 mPositionLastReportBitsUpdate = f32v3(-1.0f);
    std::unordered_set<ChunkID> mReportingChunksLookup;
    std::vector<ChunkID> mReportingChunks;
    // Represents an NxN square where true means the chunk is within the desired load range
    BitArray mPrecalculatedLoadDistanceCircle;
    i32 mPrecalculatedCircleDiameter;
    ServerPlayerID mPlayerId; // [0,MAX_PLAYERS)
    i32 mDesiredChunkLoadRange;
    i32 mDesiredChunkLoadRangeSQ;
    bool mIsHostPlayer;
};

// Keeps track of players approximate positions and their report chunks
class ServerReportStateManager {
public:
    ServerReportStateManager(i32 mWorldWidthChunks);

    void tick();

    ServerPlayerID registerPlayer(f32v3 position, int desiredChunkLoadRange);
    void unregisterPlayer(ServerPlayerID playerId);

    void updatePlayerDesiredLoadRange(ServerPlayerID playerId, int desiredChunkLoadRange);
    bool hasHostPlayer() const { return hostPlayerId != INVALID_SERVER_PLAYER_ID; }
    ServerPlayerID getHostPlayerId() const { return hostPlayerId; }

    void setPlayerPosition(ServerPlayerID playerId, f32v3 position);
    void setLocalPlayerPosition(f32v3 position);

    bool isChunkActive(ChunkID chunkId) const;

    // Returns the last known position of the player, or -FLT_MAX if the player is not found
    f32v3 getApproxPlayerPosition(ServerPlayerID playerId) const;
    // Returns invalid if no player close
    ServerPlayerID getFirstReportingPlayerToChunk(ChunkID chunkId) const;

    EVENT_LISTENER_FUNCS(
        ServerReportStateManager, PlayerRegistered, ServerPlayerEventType::PlayerRegistered, const ServerPlayerEvent&
    );
    EVENT_LISTENER_FUNCS(
        ServerReportStateManager, PlayerUnregistered, ServerPlayerEventType::PlayerUnregistered, const ServerPlayerEvent&
    );
    EVENT_LISTENER_FUNCS(
        ServerReportStateManager, PlayerLoadRangeChanged, ServerPlayerEventType::PlayerLoadRangeChanged, const ServerPlayerEvent&
    );

    EVENT_LISTENER_FUNCS_ADAPTOR(
        ServerReportStateManager, PlayerReportChanged, ServerPlayerEventType::PlayerReportChanged, const ServerPlayerReportChangedEvent&
    );
    EVENT_LISTENER_FUNCS_ADAPTOR(
        ServerReportStateManager, ChunkActiveChanged, ServerPlayerEventType::ChunkActiveChanged, const ServerPlayerChunkActiveChangeEvent&
    );
private:
    void updatePlayerLoadRangeCircle(ServerPlayerID playerId);
    void updatePlayerReportChunks(ServerPlayerID playerId);
    void setChunkReportBit(ServerPlayerID playerId, ChunkID chunkId, bool value);

    mutable std::shared_mutex mApproxPlayerPositionsMutex;
    f32v3 mApproxPlayerPositions[MAX_PLAYERS];
    // Last known positions of players when their report chunks were updated
    f32v3 mLastPlayerPositionsOnReportUpdate[MAX_PLAYERS];

    ServerPlayerData mPlayerData[MAX_PLAYERS];
    ServerPlayerID hostPlayerId = INVALID_SERVER_PLAYER_ID;
    std::vector<ServerPlayerID> mActivePlayerIds; // First is always host
    std::vector<ServerPlayerID> mFreePlayerIds;

    // If bit N is set, then the chunk at index N is being reported to the player slot represented by % MAX_PLAYERS
    BitArray mChunkReportingBits;
    // A chunk is active if it has at least one report
    std::unordered_map<ChunkID, ui8 /*reportCount*/> mActiveChunks;
    i32 mWorldWidthChunks;

    EVENT_DISPATCHER_DEF(ServerReportStateManager);
};
