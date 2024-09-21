#include "stdafx.h"
#include "ServerReportStateManager.h"

ServerReportStateManager::ServerReportStateManager(i32 worldWidthChunks) : mWorldWidthChunks(worldWidthChunks) {
    mActivePlayerIds.reserve(MAX_PLAYERS);
    mFreePlayerIds.reserve(MAX_PLAYERS);
    for (int i = MAX_PLAYERS - 1; i >= 0; i--) {
        mFreePlayerIds.push_back(ServerPlayerID(i));
    }
    mChunkReportingBits.resizeAndZero(MAX_PLAYERS * SQ(worldWidthChunks));
}

void ServerReportStateManager::tick() {
    PROFILE_FUNCTION();

    for (ServerPlayerID playerId : mActivePlayerIds) {
        ServerPlayerData& playerData = mPlayerData[playerId];

        if (glm::distance2(mApproxPlayerPositions[playerId], playerData.mPositionLastReportBitsUpdate) >= SQ(CHUNK_WIDTH)) {
            updatePlayerReportChunks(playerId);
        }
    }
}

ServerPlayerID ServerReportStateManager::registerPlayer(f32v3 position, int desiredChunkLoadRange) {
    if (mFreePlayerIds.empty()) {
        LOG_ERROR("No free player slots");
        return INVALID_SERVER_PLAYER_ID;
    }
    const bool isHost = mActivePlayerIds.empty();

    ServerPlayerID newId = mFreePlayerIds.back();
    mFreePlayerIds.pop_back();

    mActivePlayerIds.push_back(newId);

    {
        std::lock_guard lock(mApproxPlayerPositionsMutex);
        mApproxPlayerPositions[newId] = position;
    }

    ServerPlayerData newData;
    newData.mPlayerId = newId;
    newData.mDesiredChunkLoadRange = desiredChunkLoadRange;
    newData.mDesiredChunkLoadRangeSQ = SQ(desiredChunkLoadRange);
    newData.mIsHostPlayer = isHost;
    mPlayerData[newId] = newData;
    if (isHost) {
        hostPlayerId = newId;
    }

    ServerPlayerEvent evt;
    evt.playerId = newId;
    dispatchPlayerRegistered(evt);
    updatePlayerLoadRangeCircle(newId);
    updatePlayerReportChunks(newId);
    return newId;
}

void ServerReportStateManager::unregisterPlayer(ServerPlayerID playerId) {
    ASSERT_SERVER_THREAD();
    mApproxPlayerPositions[playerId] = f32v3(-FLT_MAX);

    std::remove(mActivePlayerIds.begin(), mActivePlayerIds.end(), playerId);
    mFreePlayerIds.push_back(playerId);

    ServerPlayerData& playerData = mPlayerData[playerId];
    for (ChunkID chunkId : playerData.mReportingChunks) {
        setChunkReportBit(playerId, chunkId, false);
    }
    playerData.mReportingChunks.clear();
    playerData.mReportingChunksLookup.clear();

    ServerPlayerEvent evt;
    evt.playerId = playerId;
    dispatchPlayerUnregistered(evt);
}

void ServerReportStateManager::updatePlayerDesiredLoadRange(ServerPlayerID playerId, int desiredChunkLoadRange) {
    ASSERT_SERVER_THREAD();
    ServerPlayerData& playerData = mPlayerData[playerId];
    playerData.mDesiredChunkLoadRange = desiredChunkLoadRange;
    playerData.mDesiredChunkLoadRangeSQ = SQ(desiredChunkLoadRange);

    updatePlayerLoadRangeCircle(playerId);

    ServerPlayerEvent evt;
    evt.playerId = playerId;
    evt.data = desiredChunkLoadRange;
    dispatchPlayerLoadRangeChanged(evt);
}

void ServerReportStateManager::setPlayerPosition(ServerPlayerID playerId, f32v3 position) {
    std::lock_guard lock(mApproxPlayerPositionsMutex);
    mApproxPlayerPositions[playerId] = position;
}

void ServerReportStateManager::setLocalPlayerPosition(f32v3 position) {
    std::lock_guard lock(mApproxPlayerPositionsMutex);
    mApproxPlayerPositions[0] = position;
}

bool ServerReportStateManager::isChunkActive(ChunkID chunkId) const {
    return mActiveChunks.contains(chunkId);
}

f32v3 ServerReportStateManager::getApproxPlayerPosition(ServerPlayerID playerId) const {
    std::shared_lock lock(mApproxPlayerPositionsMutex);
    return mApproxPlayerPositions[playerId];
}

ServerPlayerID ServerReportStateManager::getFirstReportingPlayerToChunk(ChunkID chunkId) const {
    ASSERT_SERVER_THREAD();
    for (ServerPlayerID playerId : mActivePlayerIds) {
        if (mPlayerData[playerId].mReportingChunksLookup.contains(chunkId)) {
            return playerId;
        }
    }
    return INVALID_SERVER_PLAYER_ID;
}

void ServerReportStateManager::updatePlayerLoadRangeCircle(ServerPlayerID playerId) {
    ServerPlayerData& playerData = mPlayerData[playerId];
    
    playerData.mPrecalculatedCircleDiameter = playerData.mDesiredChunkLoadRange * 2 + 1;
    playerData.mPrecalculatedLoadDistanceCircle.resize(SQ(playerData.mPrecalculatedCircleDiameter));
    for (int y = 0; y < playerData.mPrecalculatedCircleDiameter; y++) {
        for (int x = 0; x < playerData.mPrecalculatedCircleDiameter; x++) {
            const i32 dx = x - playerData.mDesiredChunkLoadRange;
            const i32 dy = y - playerData.mDesiredChunkLoadRange;
            const i32 distSq = SQ(dx) + SQ(dy);
            playerData.mPrecalculatedLoadDistanceCircle.setBitTo(
                y * playerData.mPrecalculatedCircleDiameter + x, distSq <= playerData.mDesiredChunkLoadRangeSQ
            );
        }
    }
}

void ServerReportStateManager::updatePlayerReportChunks(ServerPlayerID playerId) {
    PROFILE_FUNCTION();
    ServerPlayerData& playerData = mPlayerData[playerId];
    playerData.mPositionLastReportBitsUpdate = mApproxPlayerPositions[playerId];

    // We need to track which chunks are newly added, and which have been removed, so we can nofity
    // and replicate appropriately
    static std::unordered_set<ChunkID> removedReportChunks;
    removedReportChunks = playerData.mReportingChunksLookup;
    playerData.mReportingChunksLookup.clear();
    playerData.mReportingChunks.clear();

    static std::vector<ChunkID> newReportingChunks;
    newReportingChunks.clear();
    
    const int offsetToCenter = playerData.mPrecalculatedCircleDiameter / 2;
    ChunkCoord chunkPosBottomLeft = ChunkCoord::fromTilePos(mApproxPlayerPositions[playerId]);
    chunkPosBottomLeft = chunkPosBottomLeft - ChunkCoord(offsetToCenter);
    chunkPosBottomLeft.x = glm::max(chunkPosBottomLeft.x, 0);
    chunkPosBottomLeft.y = glm::max(chunkPosBottomLeft.y, 0);
    
    {
        PROFILE_SCOPE("Check");
        for (int y = 0; y < playerData.mPrecalculatedCircleDiameter; y++) {
            const i32 chunkY = chunkPosBottomLeft.y + y;
            if (chunkY >= mWorldWidthChunks) [[unlikely]] break;
            for (int x = 0; x < playerData.mPrecalculatedCircleDiameter; x++) {
                const i32 chunkX = chunkPosBottomLeft.x + x;
                if (chunkX >= mWorldWidthChunks) [[unlikely]] break;
                if (playerData.mPrecalculatedLoadDistanceCircle.getBit(y * playerData.mPrecalculatedCircleDiameter + x)) {
                    const ChunkID chunkId = chunkY * mWorldWidthChunks + chunkX;
                    mChunkReportingBits.setBitTo(chunkId, true);
                    playerData.mReportingChunks.push_back(chunkId);
                    playerData.mReportingChunksLookup.insert(chunkId);

                    // If we exist, remove us from the removedReportChunks list
                    auto it = removedReportChunks.find(chunkId);
                    if (it == removedReportChunks.end()) {
                        // If we were not in the list, we are new
                        newReportingChunks.push_back(chunkId);
                        setChunkReportBit(playerId, chunkId, true);
                    }
                    else {
                        removedReportChunks.erase(it);
                    }
                }
            }
        }
    }

    {
        PROFILE_SCOPE("SetBits");
        for (ChunkID chunkId : removedReportChunks) {
            setChunkReportBit(playerId, chunkId, false);
        }
    }

    // These chunks have been removed
    {
        PROFILE_SCOPE("Dispatch");
        ServerPlayerReportChangedEvent evt(playerId, newReportingChunks, removedReportChunks);
        dispatchPlayerReportChanged(evt);
    }
}

void ServerReportStateManager::setChunkReportBit(ServerPlayerID playerId, ChunkID chunkId, bool value) {
    mChunkReportingBits.setBitTo(chunkId * MAX_PLAYERS + playerId, value);

    auto it = mActiveChunks.find(chunkId);
    // Newly active
    if (it == mActiveChunks.end()) {
        assert(value);
        mActiveChunks.emplace(chunkId, 1);
        ServerPlayerChunkActiveChangeEvent evt(playerId, chunkId, true);
        dispatchChunkActiveChanged(evt);
    }
    else if (value) {
        ++it->second;
    }
    else {
        assert(it->second);
        --it->second;
        if (it->second == 0) {
            mActiveChunks.erase(it);
            ServerPlayerChunkActiveChangeEvent evt(playerId, chunkId, false);
            dispatchChunkActiveChanged(evt);
        }
    }
}
