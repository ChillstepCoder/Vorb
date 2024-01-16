#pragma once

#include "world/WorldContextObject.h"

#include "world/simulation/SimChunk.h"
#include "tile/TileHarvestable.h"

#include "util/BitArray.h"

class SimThread;

// What needs to simulate:
// 1. Businesses (economy) (Includes GovernmentBusiness?)
// 2. Characters
// 3. Government (Should this be a business?)
// 4. Random world events

// Can range from a simple hamlet to a sprawling metropolis

enum class SimCharacterStatus : ui8 {
    Idle,
    Working,
    Dead,
    COUNT
};

enum class SimCharacterHealth : ui8 {
    Healthy,
    LightlyWounded,
    ModeratelyWounded,
    GravelyWounded,
    Dead,
};

enum class SimCharacterTask : ui8 {
    Idle,
    Gather,
    Build,
    Travel,
    COUNT
};

class SimBuilding {
public:
    BuildingUID mUID;
};

class SimCharacter {
public:
    CityUID mResidentCityID;
    BusinessID mEmployer;
    SimCharacterTask mTask;
    SimCharacterStatus mStatus : 3;
    SimCharacterHealth mHealth : 3;
    TimestampCentisec mTaskStartTime;
    // TODO: Schedule?
    // TODO: Needs? (Let schedule handle it?)
    // Task Data
    union {
        struct {
            ChunkID targetChunk;
            TileHarvestable targetHarvestable;
        } mGatherTaskData;
        struct {
            ChunkID targetChunk;
        } mBuildTaskData;
        struct {
            ChunkID targetChunk;
        } mTravelTaskData;
    };
};
//SIZER(SimCharacter);

typedef std::vector<SimCharacter> SimCharacterList;
typedef std::vector<TimestampCentisec> TimestampList;

class SimCity {
public:
    CityUID mUID;
    entt::registry mRegistry;

    std::vector<CharacterUID> mResidents;
};

// Always active even if player is in a full chunk
struct SimPlayer {
    f32v3 mLastKnownPosition;
    ServerPlayerID mPlayerId;
};

class HostSimContext : public WorldContextObject {
public:
    HostSimContext(World& world);
    ~HostSimContext();

    void registerPlayer(ServerPlayerID playerId, f32v3 startPos);
    void setPlayerPosition(ServerPlayerID, f32v3 pos);
    void removePlayer(ServerPlayerID playerId);

private:
    //ChunkSimulator mSimulator;

    // ==================== CHUNK DATA ====================
    // Characters
    //UniqueArray<ChunkSimCharacterData> mCharactersInChunks;
    // TODO: This can be in seconds, and use ui16 with -= per frame
    std::vector<TimestampCentisec> mNextCharacterTickTimes;
    std::vector<CharacterUID> mTickingCharacters;
    std::unordered_map<CharacterUID, SimCharacter> mSimCharacters;

    // Chunks
    UniqueArray<SimChunkData> mChunkData;
    BitArray mChunkStates; // Pack SimChunkState into 2 bits per element
    BitArray mSimulatingChunks; // Chunks with SimChunkState = simulating for fast find first set bit
    // TODO: Flat set?
    std::unordered_set<ChunkID> mFullChunks; // Only store full chunks in here, usually not very many

    ui32 mSimChunkCount = 0;
    ui32 mTotalChunks;

    // TODO: Boost flat unordered map
    std::unordered_map<CityUID, SimCity> mCities;
    std::unique_ptr<SimThread> mSimThread;

    // One per player, creates load zones.
    // NOTE: For sending NPCs to full chunks, we manage them and notify the game thread when
    // it is receiving a fully simulated entity. We may still manage AI decisions here with shared data that
    // the game thread can read.
    // When full  NPCs want to come to the sim world they make a request and sleep until we respond.
    // If they are tring to join a full chunk, we will just tell them to unblock and continue simulating
    std::vector<SimPlayer> mPlayers;

    // 1 hundredth of a second (100 centiseconds = 1 second)
    // We simulate in centiseconds because we do not need fine simulation granularity
    // and we will not run out of precision unless the simulation runs for 248 days
    TimestampCentisec mSimTimeCentiseconds = 0;
};

