#pragma once

class HostSimContext;
class SimECS;
class SettlementNameContext;
class SettlementPlanner;

class SimSettlementSystem
{
public:
    SimSettlementSystem(HostSimContext& simContext, SimECS& ecs, entt::registry& registry);
    ~SimSettlementSystem();

    void tick(TimestampMs currentTime, TimestampMs deltaTime);

    bool tryCreateSettlementFromGroup(entt::entity groupEntity);
private:
    entt::entity createSettlementEntity(ChunkID rootChunk, entt::entity leader, std::vector<entt::entity>& people);
    std::unique_ptr<SettlementNameContext> mNameContext;

    entt::registry& mRegistry;
    HostSimContext& mSimContext;
    SimECS& mECS;
    TimestampMs mCurrentTime = 0;
    TimestampMs mDeltaTime = 0;
    SettlementUID mUIDGen = 0; // TODO: Serialize this

    TimestampMs mNextUpdateTime = 0;
    std::unique_ptr<SettlementPlanner> mPlanner;
};

