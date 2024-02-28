#pragma once

class World;

class SettlementPlanner
{
public:
    SettlementPlanner(World& world, entt::registry& registry);
    ~SettlementPlanner() = default;

    void onSettlementCreated(entt::entity settlementEntity, TimestampMs currentTime);
    void updatePlanner(entt::entity settlementEntity, TimestampMs currentTime, TimestampMs deltaTime);

private:
    World& mWorld;
    entt::registry& mRegistry;
    TimestampMs mLastThinkTime = 0;
};

