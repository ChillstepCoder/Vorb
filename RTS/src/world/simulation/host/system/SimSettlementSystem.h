#pragma once

class HostSimContext;
class SimECS;
class SettlementNameContext;
class SettlementPlanner;

#include "world/simulation/host/settlement/SimSettlementCharacterInterface.h"

class SimSettlementSystem
{
    friend class SimSettlementCharacterInterface;
public:
    SimSettlementSystem(HostSimContext& simContext, SimECS& ecs, entt::registry& registry);
    ~SimSettlementSystem();

    void tick(TimestampMs currentTime, TimestampMs deltaTimeMs);

    bool tryCreateSettlementFromGroup(entt::entity groupEntity);

    SimSettlementCharacterInterface& getCharacterInterface() { return mCharacterInterface; }
private:
    entt::entity createSettlementEntity(ChunkID rootChunk, entt::entity leader, std::vector<entt::entity>& people);
    std::unique_ptr<SettlementNameContext> mNameContext;

    entt::registry& mRegistry;
    HostSimContext& mSimContext;
    SimECS& mECS;
    TimestampMs mCurrentTime = 0;
    TimestampMs mDeltaTimeMs = 0;
    f32 mDeltaTimeSec = 0;
    SettlementUID mUIDGen = 0; // TODO: Serialize this

    TimestampMs mNextUpdateTime = 0;
    std::unique_ptr<SettlementPlanner> mPlanner;

    SimSettlementCharacterInterface mCharacterInterface;
};

