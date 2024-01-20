#pragma once

class HostSimContext;
class SimWorldAnalytics;

class SimImmigrationManager
{
public:
    SimImmigrationManager(HostSimContext& simContext);
    ~SimImmigrationManager();

    void tickSimThread(TimestampMs currentTime);

private:
    void spawnImmigrationBySea(TimestampMs currentTime);

    HostSimContext& mHostSimContext;
    TimestampMs mLastImmigrationTimestamp = 0;
    SimWorldAnalytics& mWorldAnalytics;
};

