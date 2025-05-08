#pragma once

class World;
class SimECS;

class SettlementPlanner
{
public:
    SettlementPlanner(World& world, SimECS& ecs, entt::registry& registry);
    ~SettlementPlanner() = default;

    void onSettlementCreated(entt::entity settlementEntity, TimestampMs currentTime);
    void updatePlanner(entt::entity settlementEntity, TimestampMs currentTime, TimestampMs deltaTime);

private:
    void updateResidentsPendingHomes(entt::entity settlementEntity);

    World& mWorld;
    entt::registry& mRegistry;
    SimECS& mEcs;
    TimestampMs mLastThinkTime = 0;
};

