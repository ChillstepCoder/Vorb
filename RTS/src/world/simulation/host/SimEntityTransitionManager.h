#pragma once

#include "ecs/component/FullEntityBindingComponent.h"
#include "ecs/ChunkFullTransitionData.h"

class SimECS;
class HostSimContext;

// Static class that helps manage transitions between sim entities and full entities
class SimEntityTransitionManager {
public:
    SimEntityTransitionManager(HostSimContext& simContext) : mContext(simContext) {}

    void tickSimThread(TimestampMs simTime);

    // ====================================================================
    // Sim Entities
    // ====================================================================
    void markSimEntityForTransition(entt::entity entity);
    void markChunkSimEntitiesForTransition(ChunkID chunkId);
    void initiateSimTransitions();

    void transitionEntitiesToSimFromFull(ChunkID chunkId, ChunkSimTransitionData& transitionData);
    void transitionEntityToSimFromFull(ChunkID chunkId, EntitySimTransitionData& transitionData);

    // Can happen if a chunk is deactivated after we send off entities to be activated
    void onEntityFullTransitionFailed(ChunkID chunkId, ChunkFullTransitionData&& activateData);

    // Can happen if the chunk is simulating when the game thread receives the entity
    void onEntitySimTransitionFailed(ChunkID chunkId, const EntitySimTransitionData& deactivateEntity);
private:

    // Creates necessary data and destroys component on sim entity
    EntityFullTransitionData prepareEntityForFullTransition(entt::entity entity);
    EntityFullTransitionData prepareCharacterEntityForFullTransition(entt::entity entity);

    // ====================================================================
    // Data
    // ====================================================================
    // For batch send to game thread
    HostSimContext& mContext;
    UnorderedFlatMap<ChunkID, ChunkFullTransitionData> mQueuedFullTransitions;

    // Guarantee pointer stability for entity bindings via std::unordered_map
    std::unordered_map<entt::entity, SimFullEntityBinding> mFullEntityBindings;
};

