#pragma once

#include "ecs/EntityFullActivateData.h"

class HostSimContext;
class SimECS;
class World;

struct SimBrainComponent;
struct SimInProgressTaskComponent;
struct SimPositionComponent;

class SimAISystem {
public:
    SimAISystem(HostSimContext& simContext, SimECS& ecs, entt::registry& registry);

    void tick(TimestampMs currentTime, TimestampMs deltaTime);
    void setEntityPosition(entt::entity e, f32v2 newPosition);

private:
    void updateCharacterGroups();
    void handleTaskComplete(entt::entity entity, SimBrainComponent& brain, SimInProgressTaskComponent& taskCmp);
    void updateFollowCharacterGroup(entt::entity entity, SimBrainComponent& brain, SimPositionComponent& pos);

    World& mWorld;
    entt::registry& mRegistry;
    HostSimContext& mSimContext;
    SimECS& mECS;
    TimestampMs mCurrentTime = 0;
    TimestampMs mDeltaTime = 0;
    ui32 mWorldWidthChunks = 0;
};

