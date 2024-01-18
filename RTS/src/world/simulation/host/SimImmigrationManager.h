#pragma once

class HostSimContext;

class SimImmigrationManager
{
public:
    SimImmigrationManager(HostSimContext& simContext);
    ~SimImmigrationManager();

    void tickSimThread(TimestampMs currentTime);

private:
    HostSimContext& mHostSimContext;
    TimestampMs mLastTickTimestamp = 0;
};

