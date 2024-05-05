#pragma once

#include "world/simulation/host/SimECSEvents.h"
#include "ecs/EntityFullActivateData.h"

class HostSimContext;
class SimECS;
class World;

struct SimBrainComponent;
struct SimInProgressTaskComponent;
struct SimPositionComponent;

typedef std::vector<entt::entity> SimChunkEntityList;

class SimAISystem {
public:
    SimAISystem(HostSimContext& simContext, SimECS& ecs, entt::registry& registry);

    void tick(TimestampMs currentTime, TimestampMs deltaTime);
    void setEntityPosition(entt::entity e, f32v2 newPosition);

    // Transition our AI entities to fully simulated and return the list of AI entitiess
    ChunkEntityFullActivateDataList simThreadOnActivateChunk(ChunkID chunk);
private:
    void updateCharacterGroups();
    void handleTaskComplete(entt::entity entity, SimBrainComponent& brain, SimInProgressTaskComponent& taskCmp);
    void updateFollowCharacterGroup(entt::entity entity, SimBrainComponent& brain, SimPositionComponent& pos);
    void onEntityEnterNewChunk(entt::entity entity, ChunkID prevChunk, ChunkID newChunk);

    World& mWorld;
    entt::registry& mRegistry;
    HostSimContext& mSimContext;
    SimECS& mECS;
    TimestampMs mCurrentTime = 0;
    TimestampMs mDeltaTime = 0;
    ui32 mWorldWidthChunks = 0;

    std::vector<SimChunkEntityList> mAIEntitiesInChunks;
    SimECSListeners mECSEventListeners;
};

