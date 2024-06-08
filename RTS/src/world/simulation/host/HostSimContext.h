#pragma once

#include "world/WorldContextObject.h"

#include "world/simulation/host/SimWorldAnalytics.h"
#include "world/ChunkGridEvent.h"
#include "ecs/FullECSEvents.h"

#include "world/simulation/host/SimEntityTransitionManager.h"

#include "tile/TileHarvestable.h"

#include "util/BitArray.h"

#include <Vorb/concurrentqueue.h>

class SimThread;
class StoryTeller;
class SimECS;
class SimImmigrationManager;
class RandomGenerator;

// What needs to simulate:
// 1. Businesses (economy) (Includes GovernmentBusiness?)
// 2. Characters
// 3. Government (Should this be a business?)
// 4. Random world events

// Can range from a simple hamlet to a sprawling metropolis


// Always active even if player is in a full chunk
struct SimPlayer {
    f32v3 mLastKnownPosition;
    ServerPlayerID mPlayerId;
};

class HostSimContext : public WorldContextObject {
    friend class SimThread;
public:
    HostSimContext(World& world);
    ~HostSimContext();

    void beginHistorySimulation();
    void endHistorySimulation();

    void onWorldBeginGame();

    void registerPlayer(ServerPlayerID playerId, f32v3 startPos);
    void setPlayerPosition(ServerPlayerID, f32v3 pos);
    void removePlayer(ServerPlayerID playerId);

    void addSimThreadTask(std::function<void()> task);

    SimThread* tryGetSimThread() const { return mSimThread.get(); }
    TimestampMs getSimTime() const { return mSimTime; }
    SimECS& getECS() const { return *mSimECS; }
    SimWorldAnalytics& getAnalytics() const { return *mAnalytics; }
    RandomGenerator& getSimRandomGenerator() const;
    ui32 getWidthChunks() const;
    bool isChunkSimulating(ChunkID chunkId) const;

    SimEntityTransitionManager& getEntityTransitionManager() const { return *mEntityTransitionManager; }

    // DEBUGGING
    void debugRender(f32v3 cameraPos) const;

private:
    void initEvents();
    //ChunkSimulator mSimulator;

    // ==================== CHUNK DATA ====================
    // Characters
    //UniqueArray<ChunkSimCharacterData> mCharactersInChunks;
    // TODO: This can be in seconds, and use ui16 with -= per frame

    // Chunks
    BitArray mChunkStates; // Pack SimChunkState into 2 bits per element
    BitArray mSimulatingChunks; // Chunks with SimChunkState = simulating for fast find first set bit
    // TODO: Flat set?
    std::unordered_set<ChunkID> mFullChunks; // Only store full chunks in here, usually not very many
    ChunkGridListeners mChunkEventListeners;
    IFullECSListeners mFullECSListeners;

    ui32 mTotalChunks;

    // TODO: Boost flat unordered map
    std::unique_ptr<SimThread> mSimThread;
    std::unique_ptr<SimEntityTransitionManager> mEntityTransitionManager;
    std::unique_ptr<StoryTeller> mStoryTeller;
    std::unique_ptr<SimECS> mSimECS;
    std::unique_ptr<SimImmigrationManager> mImmigrationManager;
    std::unique_ptr<SimWorldAnalytics> mAnalytics;

    // One per player, creates load zones.
    // NOTE: For sending NPCs to full chunks, we manage them and notify the game thread when
    // it is receiving a fully simulated entity. We may still manage AI decisions here with shared data that
    // the game thread can read.
    // When full  NPCs want to come to the sim world they make a request and sleep until we respond.
    // If they are tring to join a full chunk, we will just tell them to unblock and continue simulating
    std::vector<SimPlayer> mPlayers;

    std::atomic<TimestampMs> mSimTime = 0; // Time in ms since the world started, should sync with world
    bool mSimulatingHistory = false;
};

