#include "stdafx.h"
#include "SimEntityTransitionManager.h"

#include "world/World.h"
#include "world/simulation/host/component/SimCharacterComponents.h"
#include "world/simulation/host/SimECS.h"
#include "world/simulation/host/HostSimContext.h"
#include "world/simulation/host/SimThread.h"
#include "world/chunk/SimChunkGrid.h"
#include "ecs/component/FullEntityBindingComponent.h"
#include "ecs/IFullECS.h"

#include "ecs/component/SimEntityTypeComponent.h"

#include "gamethread/GameThreadTasks.h"
#include "world/IChunkGrid.h"

void SimEntityTransitionManager::tickSimThread(TimestampMs simTime) {
    ASSERT_SIM_THREAD();
    // Send newly activated entities to game thread
    if (mQueuedFullTransitions.size()) {
        // We will pause all execution while the game thread is processing the tasks
        // so it can freely access our data to construct entities
        initiateSimTransitions();

    }
}

void SimEntityTransitionManager::markSimEntityForTransition(entt::entity entity) {
    ASSERT_SIM_THREAD();

    const ChunkID chunkId = mContext.getECS().mRegistry.get<SimPositionComponent>(entity).getChunk();
    mQueuedFullTransitions[chunkId].entities.emplace_back(prepareEntityForFullTransition(entity));
}

void SimEntityTransitionManager::markChunkSimEntitiesForTransition(ChunkID chunkId) {
    ASSERT_SIM_THREAD();

    SimECS& ecs = mContext.getECS();

    ChunkFullTransitionData& activateData = mQueuedFullTransitions[chunkId];
    std::vector<EntityFullTransitionData>& activateEntities = activateData.entities;

    EntityVector& chunkEntities = ecs.mEntitiesInChunks[chunkId];
    activateEntities.reserve(chunkEntities.size() + activateEntities.size());

    for (size_t i = 0; i < chunkEntities.size(); ++i) {
        assert(ecs.mRegistry.get<SimPositionComponent>(chunkEntities[i]).getChunk() == chunkId);
        assert(ecs.mWorld.getChunkIDAtWorldPos(ecs.mRegistry.get<SimPositionComponent>(chunkEntities[i]).getPosition()) == chunkId);
        activateEntities.emplace_back(prepareEntityForFullTransition(chunkEntities[i]));
    }

    // Free memory
    // TODO: This mutation of ecs state is not ideal, hard to tell from ecs class how this changes
    chunkEntities.clear();
    if (chunkEntities.capacity() > ENTITY_LIST_DEALLOCATE_COUNT) {
        chunkEntities.shrink_to_fit();
        chunkEntities.reserve(ENTITY_LIST_RESERVE_COUNT);
    }

    SimChunk& simChunk = mContext.getWorld().getSimChunkGrid().getChunk(chunkId);
    activateData.itemStacks = simChunk.getItemDataCopy();
}

void SimEntityTransitionManager::initiateSimTransitions() {
    PROFILE_FUNCTION();
    assert(mQueuedFullTransitions.size() > 0);

    ASSERT_SIM_THREAD();
    for (auto& [chunkId, data] : mQueuedFullTransitions) {
        auto dataPtr = std::make_shared<ChunkFullTransitionData>(std::move(data));
        GameThreadTasks::getInstance().addGenericTask([this, &ecs = mContext.getECS(), chunkId, dataPtr=std::move(dataPtr)]() mutable {
            Chunk& chunk = ecs.mWorld.getChunkGrid().getChunk(chunkId);
            if (chunk.isActivated()) {
                ecs.mWorld.getECS().createFullEntitiesFromSimEntities(chunk, *dataPtr);
            }
            else if (chunk.isDeactivated()) {
                // Rare case where chunk deactivated when we were trying to send it entities, so we need to send them back
                ecs.mHostSimContext.tryGetSimThread()->addTask([this, chunkId, dataPtr = std::move(dataPtr)]() mutable {
                    onEntityFullTransitionFailed(chunkId, std::move(*dataPtr));
                });
            }
            else {
                // If here, we are in the process of activating, so mark as pending
                ecs.mWorld.getECS().addPendingEntitiesToChunk(chunk, std::move(*dataPtr));
            }
        });
    }

    mQueuedFullTransitions.clear();
}

void SimEntityTransitionManager::transitionEntitiesToSimFromFull(ChunkID chunkId, ChunkSimTransitionData& transitionData) {
    ASSERT_SIM_THREAD();

    SimECS& ecs = mContext.getECS();
    EntityVector& list = ecs.mEntitiesInChunks[chunkId];
    list.reserve(list.size() + transitionData.entities.size());
    for (EntitySimTransitionData& dd : transitionData.entities) {
        SimPositionComponent& posCmp = ecs.mRegistry.emplace<SimPositionComponent>(dd.simEntity);
        ecs.mRegistry.emplace<SimMovementComponent>(dd.simEntity);
        posCmp.position = dd.simPosition;
        posCmp.chunk = chunkId;
        list.emplace_back(dd.simEntity);

        dd.characterData->moveToEntity(mContext.getWorld(), ecs.mRegistry, dd.simEntity, false /*isFull*/);

        // Process and remove binding once entity is fully constructed and can process any tasks properly
        FullEntityBindingComponent& binding = ecs.mRegistry.get<FullEntityBindingComponent>(dd.simEntity);
        binding.binding->processSimThreadPreRemove(ecs.mRegistry, dd.simEntity);
        binding.binding->decRefCount();

        if (binding.binding->getRefCount() == 0) {
            auto&& it = mFullEntityBindings.find(dd.simEntity);
            assert(&it->second == binding.binding);

            mFullEntityBindings.erase(it);
            ecs.mRegistry.remove<FullEntityBindingComponent>(dd.simEntity);
        }
    }
}

void SimEntityTransitionManager::transitionEntityToSimFromFull(ChunkID chunkId, EntitySimTransitionData& transitionData) {
    ASSERT_SIM_THREAD();

    SimECS& ecs = mContext.getECS();
    EntityVector& list = ecs.mEntitiesInChunks[chunkId];
    SimPositionComponent& posCmp = ecs.mRegistry.emplace<SimPositionComponent>(transitionData.simEntity);
    ecs.mRegistry.emplace<SimMovementComponent>(transitionData.simEntity);
    posCmp.position = transitionData.simPosition;
    posCmp.chunk = chunkId;
    list.emplace_back(transitionData.simEntity);

    // Process and remove binding
    FullEntityBindingComponent& binding = ecs.mRegistry.get<FullEntityBindingComponent>(transitionData.simEntity);
    binding.binding->processSimThreadPreRemove(ecs.mRegistry, transitionData.simEntity);
    binding.binding->decRefCount();

    if (binding.binding->getRefCount() == 0) {
        auto&& it = mFullEntityBindings.find(transitionData.simEntity);
        assert(&it->second == binding.binding);

        mFullEntityBindings.erase(it);
        ecs.mRegistry.remove<FullEntityBindingComponent>(transitionData.simEntity);
    }

    transitionData.characterData->moveToEntity(mContext.getWorld(), ecs.mRegistry, transitionData.simEntity, false /*isFull*/);
}

void SimEntityTransitionManager::onEntityFullTransitionFailed(ChunkID chunkId, ChunkFullTransitionData&& activateData) {
    ASSERT_SIM_THREAD();

    SimECS& ecs = mContext.getECS();
    if (ecs.mHostSimContext.isChunkSimulating(chunkId)) {
        ChunkSimTransitionData data;
        data.entities.resize(activateData.entities.size());
        for (size_t i = 0; i < activateData.entities.size(); ++i) {
            data.entities[i].simEntity = activateData.entities[i].binding->simEntity;
            data.entities[i].simPosition = activateData.entities[i].simPosition;
        }
        transitionEntitiesToSimFromFull(chunkId, data);
    }
    else {
        // Fail again! either due to delay or due to chunk immediately reactivating, just keep ping ponging back till it owrks
        ChunkFullTransitionData& list = mQueuedFullTransitions[chunkId];
        if (list.entities.empty()) {
            list.entities.swap(activateData.entities);
        }
        else {
            // Append
            list.entities.reserve(list.entities.size() + activateData.entities.size());
            for (auto&& activateData : activateData.entities) {
                list.entities.emplace_back(std::move(activateData));
            }
        }
    }
}

void SimEntityTransitionManager::onEntitySimTransitionFailed(ChunkID chunkId, const EntitySimTransitionData& deactivateEntity) {
    ASSERT_SIM_THREAD();

    auto&& it = mFullEntityBindings.find(deactivateEntity.simEntity);
    assert(it != mFullEntityBindings.end());

    SimECS& ecs = mContext.getECS();
    EntityFullTransitionData& activateData = mQueuedFullTransitions[chunkId].entities.emplace_back();
    activateData.entityType = ecs.mRegistry.get<SimEntityTypeComponent>(deactivateEntity.simEntity).type;
    activateData.simPosition = deactivateEntity.simPosition;
    activateData.binding = &it->second;

}

EntityFullTransitionData SimEntityTransitionManager::prepareEntityForFullTransition(entt::entity entity) {
    ASSERT_SIM_THREAD();

    SimECS& ecs = mContext.getECS();
    switch (ecs.mRegistry.get<SimEntityTypeComponent>(entity).type) {
        case SimEntityType::Character:
            return prepareCharacterEntityForFullTransition(entity);
        default:
            panic("Invalid entity type {} in SimEntityTransitionManager::prepareEntityForSimTransition", (int)ecs.mRegistry.get<SimEntityTypeComponent>(entity).type);
    }
}

EntityFullTransitionData SimEntityTransitionManager::prepareCharacterEntityForFullTransition(entt::entity entity) {
    SimECS& ecs = mContext.getECS();
    EntityFullTransitionData rv;

    // Create binding
    SimFullEntityBinding& binding = mFullEntityBindings[entity];
    binding.simEntity = entity;
    binding.incRefCount();

    rv.moveFromSimEntity(ecs.mRegistry, entity, binding);

    // Creating the binding will fully mark us as a "full" entity, and our entity operations will go to the game thread
    ecs.mRegistry.get_or_emplace<FullEntityBindingComponent>(entity).binding = &binding;
    return rv;
}
