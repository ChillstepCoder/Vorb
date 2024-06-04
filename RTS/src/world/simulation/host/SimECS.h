#pragma once

#include "world/simulation/host/CharacterGroupType.h"
#include "world/simulation/host/SimECSEvents.h"
#include "world/simulation/host/SimEntityTransitionManager.h"

class HostSimContext;
class SimAISystem;
class SimSettlementSystem;
class World;

enum class CharacterGroupDissolveReason : ui8 {
    None,
    GoalSuccess,
    Disbanded,
    Merged
};

constexpr ui32 ENTITY_LIST_RESERVE_COUNT = 64;
// Prevent lists getting too out of control
constexpr ui32 ENTITY_LIST_DEALLOCATE_COUNT = 512;

// Host only, owned by SimThread
class SimECS
{
public:
    friend class SimEntityTransitionManager;

public:
    SimECS(HostSimContext& hostSimContext);
    ~SimECS();

    void tickSimThread(TimestampMs currentTimestamp);
    // Allocates bare minimum components
    // TODO: entity factory
    entt::entity createNewPerson(f32v2 worldTilePosition);
    entt::registry& getRegistrySimThread() { ASSERT_SIM_THREAD(); return mRegistry; }
    // Returns the group entity
    entt::entity createNewSettlerCaravan(std::span<entt::entity> members, int leaderIndex, ChunkID targetChunk);
    // Destroys the group entity and triggers members to resolve the group condition
    void endCharacterGroup(entt::entity group, CharacterGroupDissolveReason reason);

    SimAISystem& getAISystem() { return *mAISystem; }
    SimSettlementSystem& getSettlementSystem(){ return *mSettlementSystem; }
    World& getWorld() const { return mWorld; }

    void simThreadOnActivateChunk(ChunkID id);

    EVENT_LISTENER_FUNCS(SimECS, EntityCreated, SimECSEventType::EntityCreated, SimECSEvent);
    EVENT_LISTENER_FUNCS(SimECS, EntityDestroyed, SimECSEventType::EntityDestroyed, SimECSEvent);

    // Returns true if our entity is fully activating, if returned true, position and movement are INVALID
    bool onEntityEnterNewChunk(entt::entity entity, ChunkID prevChunk, ChunkID newChunk);

    // DEBUGGING
    void debugRender(f32v3 cameraPos) const;
    
    mutable std::mutex mDebugDrawMutex;
    struct DebugDrawSimAgentData {
        f32v3 pos;
        color4 color;
    };
    mutable std::vector<DebugDrawSimAgentData> mDebugDrawAgents[2]; // Double buffer
    void addDebugDrawData(DebugDrawSimAgentData data) { ASSERT_SIM_THREAD(); mDebugDrawAgents[1].emplace_back(data); }
private:
    void debugRenderInternal() const;
    entt::entity createNewCharacterGroup(std::span<entt::entity> members, int leaderIndex, CharacterGroupType groupType);

    void onEntityDestroyed(entt::entity entity, SimEntityType type);

    SimEntityTransitionManager& mEntityTransitioner;

    entt::registry mRegistry;
    TimestampMs mCurrentTickTimestamp = 0;
    TimestampMs mTimeDelta = 0;
    TimestampMs mLastTickTimestamp = 0;
    HostSimContext& mHostSimContext;
    World& mWorld;

    // TODO: Serialize this
    CharacterUID mUIDGenerator = 0;

    std::unique_ptr<SimAISystem> mAISystem;
    std::unique_ptr<SimSettlementSystem> mSettlementSystem;

    // TODO: Farm plots, ect
    std::vector<EntityVector> mEntitiesInChunks;
    //std::vector<SimChunkEntityList> mStaticEntitiesInChunks;

    // TODO: Strip in release?
    mutable std::mutex mDebugRenderMutex;
    mutable f32v3 mDebugCameraPos = f32v3(FLT_MAX);

    EVENT_DISPATCHER_DEF(SimECS);
};

