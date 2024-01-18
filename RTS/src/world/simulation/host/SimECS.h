#pragma once

class HostSimContext;

// Host only, owned by SimThread
class SimECS
{
public:
    SimECS(HostSimContext& hostSimContext);
    ~SimECS();

    void tickSimThread(TimestampMs currentTimestamp);

private:
    void updateAI();
    void updateSettlements();

    entt::registry mRegistry;
    TimestampMs mCurrentTickTimestamp = 0;
    TimestampMs mTimeDelta = 0;
    TimestampMs mLastTickTimestamp = 0;
    HostSimContext& mHostSimContext;
};

