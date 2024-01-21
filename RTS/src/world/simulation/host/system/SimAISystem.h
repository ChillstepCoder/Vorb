#pragma once

class HostSimContext;
class SimECS;

struct SimBrainComponent;
struct SimInProgressTaskComponent;
struct SimPositionComponent;

class SimAISystem {
public:
    SimAISystem(HostSimContext& simContext, entt::registry& registry);

    void tick(TimestampMs currentTime, TimestampMs deltaTime);

private:
    void updateCharacterGroups();
    void handleTaskComplete(entt::entity entity, SimBrainComponent& brain, SimInProgressTaskComponent& taskCmp);
    void updateFollowCharacterGroup(entt::entity entity, SimBrainComponent& brain, SimPositionComponent& pos);

    entt::registry& mRegistry;
    HostSimContext& mSimContext;
    SimECS& mECS;
    TimestampMs mCurrentTime = 0;
    TimestampMs mDeltaTime = 0;
};

