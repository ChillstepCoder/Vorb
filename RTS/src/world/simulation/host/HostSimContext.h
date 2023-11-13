#pragma once

#include "world/WorldContextObject.h"

#include "world/simulation/SimChunk.h"
#include "tile/TileHarvestable.h"

#include "util/BitArray.h"

// What needs to simulate:
// 1. Businesses (economy) (Includes GovernmentBusiness?)
// 2. Characters
// 3. Government (Should this be a business?)
// 4. Random world events

// Can range from a simple hamlet to a sprawling metropolis

typedef ui16 BusinessID; // No more than 65535 businesses per city
typedef ui32 CityUID; // We dont make many cities so ui32 is fine. We can always change it later
typedef ui64 CharacterUID;
typedef ui64 BuildingUID;
typedef i32 TimestampCentisec;

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


class HostSimContext : public WorldContextObject {
public:
    HostSimContext(World& world);

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
    UniqueArray<SimChunkState> mStates;
    UniqueArray<SimChunkData> mData;
    BitArray mSimulatingChunks;

    ui32 mSimChunkCount = 0;

    // TODO: Boost flat unordered map
    std::unordered_map<CityUID, SimCity> mCities;


    // 1 hundredth of a second (100 centiseconds = 1 second)
    // We simulate in centiseconds because we do not need fine simulation granularity
    // and we will not run out of precision unless the simulation runs for 248 days
    TimestampCentisec mSimTimeCentiseconds = 0;
};

