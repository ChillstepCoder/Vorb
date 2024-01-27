#pragma once

class HostSimContext;
class SimWorldAnalytics;

#include <boost/container/flat_map.hpp>

struct BodyImmigrationData {
    std::vector<ChunkID> mPrioritySortedChunks; // Highest priority at end for pop_back
    std::vector<ChunkID> mOccupiedChunks; // Chunks move from the priority list to here when occupied
    ui32 mTotalSettlementsSent = 0;
};

class SimImmigrationManager
{
public:
    SimImmigrationManager(HostSimContext& simContext);
    ~SimImmigrationManager();

    // Requires markup complete
    void init();
    void tickSimThread(TimestampMs currentTime);

    // TODO:  - Need to listen for ownership change event on the OwnershipGrid?
private:

    struct ImmigrationOrder {
        ChunkID startChunk = INVALID_CHUNK_ID;
        ChunkID targetChunk = INVALID_CHUNK_ID;
    };

    ImmigrationOrder getNextImmigrationOrder();

    void spawnImmigrationBySea(TimestampMs currentTime);


    HostSimContext& mHostSimContext;
    WorldMarkupGrid& mMarkupGrid;
    TimestampMs mLastImmigrationTimestamp = 0;
    SimWorldAnalytics& mWorldAnalytics;
    boost::container::flat_map<BodyID, BodyImmigrationData> mImmigrationData;
};

