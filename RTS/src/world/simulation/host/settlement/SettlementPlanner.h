#pragma once
class SettlementPlanner
{
public:
    SettlementPlanner(entt::registry& registry);
    ~SettlementPlanner() = default;

    void onSettlementCreated(entt::entity settlementEntity, TimestampMs currentTime);
    void updatePlanner(entt::entity settlementEntity, TimestampMs currentTime, TimestampMs deltaTime);

private:
    entt::registry& mRegistry;
    TimestampMs mLastThinkTime = 0;
};

