#pragma once

#include "world/simulation/host/CharacterGroupType.h"
#include "world/simulation/host/SimECSEvents.h"
#include "ecs/EntityFullActivateData.h"

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

// Host only, owned by SimThread
class SimECS
{
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

    // Transition our AI entities to fully simulated and return the list of AI entitiess
    ChunkEntityFullActivateDataList simThreadOnActivateChunk(ChunkID chunkId);
    void simThreadOnFullDeactivateEntities(ChunkID chunkId, const ChunkEntityFullDeactivateDataList& deactivateEntities);
    void simThreadOnFullDeactivateEntity(ChunkID chunkId, const EntityFullDeactivateData& deactivateEntitity);
    // For when we cannot deactivate an entity as we do not have control, send it back to game thread
    void onEntityDeactivationFailed(ChunkID chunkId, const EntityFullDeactivateData& deactivateEntity);

    EVENT_LISTENER_FUNCS(SimECS, EntityCreated, SimECSEventType::EntityCreated, SimECSEvent);
    EVENT_LISTENER_FUNCS(SimECS, EntityDestroyed, SimECSEventType::EntityDestroyed, SimECSEvent);

    // Returns true if our entity is fully activating, if returned true, position and movement are INVALID
    bool onEntityEnterNewChunk(entt::entity entity, ChunkID prevChunk, ChunkID newChunk);

    // DEBUGGING
    void debugRender(f32v3 cameraPos) const;
private:
    EntityFullActivateData onFullActivateEntity(entt::entity entity);
    // Can happen if a chunk is deactivated after we send off entities to be activated
    void onEntityFullActivationFailed(ChunkID chunkId, ChunkEntityFullActivateDataList&& activateData);
    void debugRenderInternal() const;
    entt::entity createNewCharacterGroup(std::span<entt::entity> members, int leaderIndex, CharacterGroupType groupType);

    void onEntityDestroyed(entt::entity entity, SimEntityType type);

    // For batch send to game thread
    std::unordered_map<ChunkID, std::vector<EntityFullActivateData>> mFullActivatedEntitiesThisFrame;

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

    // Guarantee pointer stability for entity bindings
    std::unordered_map<entt::entity, SimFullEntityBinding> mFullEntityBindings;

    // TODO: Farm plots, ect
    std::vector<EntityVector> mEntitiesInChunks;
    //std::vector<SimChunkEntityList> mStaticEntitiesInChunks;

    // TODO: Strip in release?
    mutable std::mutex mDebugRenderMutex;
    mutable f32v3 mDebugCameraPos = f32v3(FLT_MAX);

    EVENT_DISPATCHER_DEF(SimECS);
};

