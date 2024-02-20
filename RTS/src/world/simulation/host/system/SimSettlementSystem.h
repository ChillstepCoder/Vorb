#pragma once

class HostSimContext;
class SimECS;
class SettlementNameContext;

class SimSettlementSystem
{
public:
    SimSettlementSystem(HostSimContext& simContext, SimECS& ecs, entt::registry& registry);

    void tick(TimestampMs currentTime, TimestampMs deltaTime);

    bool tryCreateSettlementFromGroup(entt::entity groupEntity);
private:

    std::unique_ptr<SettlementNameContext> mNameContext;

    entt::registry& mRegistry;
    HostSimContext& mSimContext;
    SimECS& mECS;
    TimestampMs mCurrentTime = 0;
    TimestampMs mDeltaTime = 0;
};

