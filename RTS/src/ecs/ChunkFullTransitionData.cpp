#include "stdafx.h"
#include "ChunkFullTransitionData.h"

#include "ecs/component/Components.h"
#include "ecs/component/FullEntityBindingComponent.h"

POOLED_ALLOC_DEF_THREADSAFE(EntityComponentCharacterTransitionData, 256);


void EntityOperations::simPerformDual(entt::registry& simRegistry, entt::entity entity, EntityOperationFunc func) {
    ASSERT_SIM_THREAD();
    if (FullEntityBindingComponent* bindingCmp = simRegistry.try_get<FullEntityBindingComponent>(entity)) {
        // If we are on the game thread, add operation
        bindingCmp->binding->simAddOperation(func);
    } else {
        // Otherwise process here
        func(simRegistry, entity, false /*isGameThread*/);
    }
}


void EntityComponentCharacterTransitionData::moveFromEntity(entt::registry& registry, entt::entity entity) {
    assert(!isValid);
    isValid = true;

    character = std::move(registry.get<DualCharacterComponent>(entity));

    taskQueue = std::move(registry.get<DualTaskQueueComponent>(entity));
    registry.remove<DualTaskQueueComponent>(entity);

    attributes = std::move(registry.get<DualAttributesComponent>(entity));
    registry.remove<DualAttributesComponent>(entity);

    inventory = std::move(registry.get<DualInventoryComponent>(entity));
    registry.remove<DualInventoryComponent>(entity);

    gender = std::move(registry.get<DualGenderComponent>(entity));
    registry.remove<DualGenderComponent>(entity);

    resident = std::move(registry.get<DualResidentComponent>(entity));
    registry.remove<DualResidentComponent>(entity);
}

void EntityComponentCharacterTransitionData::moveToEntity(World& world, entt::registry& registry, entt::entity entity, bool isFull) {
    assert(isValid);
    isValid = false;
    registry.emplace_or_replace<DualCharacterComponent>(entity, std::move(character));
    registry.emplace<DualAttributesComponent>(entity, std::move(attributes));
    registry.emplace<DualInventoryComponent>(entity, std::move(inventory));
    registry.emplace<DualGenderComponent>(entity, std::move(gender));
    registry.emplace<DualResidentComponent>(entity, std::move(resident));

    DualTaskQueueComponent& newTaskQueue = registry.emplace<DualTaskQueueComponent>(entity, std::move(taskQueue));
    // Notify tasks of the transition
    if (isFull) {
        for (SimTaskHandle& handle : newTaskQueue.taskQueue) {
            handle.onTransitionToFull(world, registry, entity);
        }
    }
    else {
        for (SimTaskHandle& handle : newTaskQueue.taskQueue) {
            handle.onTransitionToSim(world, registry, entity);
        }
    }
}

void EntitySimTransitionData::moveFromFullEntity(entt::registry& registry, entt::entity entity) {
    // TODO: NOT JUST CHARACTER
    entityType = SimEntityType::Character;
    simEntity = registry.get<FullEntityBindingComponent>(entity).binding->simEntity;
    simPosition = registry.get<PositionComponent>(entity).mPosition;
    characterData = std::make_unique<EntityComponentCharacterTransitionData>();
    characterData->moveFromEntity(registry, entity);
}

void EntityFullTransitionData::moveFromSimEntity(entt::registry& registry, entt::entity entity, SimFullEntityBinding& entityBinding) {
    assert(entityBinding.simEntity == entity);
    // TODO: NOT JUST CHARACTER
    entityType = SimEntityType::Character;
    SimPositionComponent& posCmp = registry.get<SimPositionComponent>(entity);
    simPosition = posCmp.getPosition();

   // TODO: Store bindings here?
    binding = &entityBinding;

    characterData = std::make_unique<EntityComponentCharacterTransitionData>();
    characterData->moveFromEntity(registry, entity);

    // Erase components
    // We erase our sim position and movement while fully activated
    registry.remove<SimPositionComponent>(entity);
    registry.remove<SimMovementComponent>(entity);
}

void SimFullEntityBinding::processGameThread(entt::registry& registry, entt::entity entity) {
    ASSERT_GAME_THREAD();
    if (hasQueuedOperations.load()) {
        std::lock_guard lock(mutex);
        while (!queuedOperations.empty()) {
            queuedOperations.front()(registry, entity, true /*isGameThread*/);
            queuedOperations.pop();
        }
        hasQueuedOperations = false;
    }
}

void SimFullEntityBinding::processSimThreadPreRemove(entt::registry& registry, entt::entity entity) {
    ASSERT_SIM_THREAD();
    // No lock needed as the game thread has already lost its handle
    while (!queuedOperations.empty()) {
        queuedOperations.front()(registry, entity, false /*isGameThread*/);
        queuedOperations.pop();
    }
    hasQueuedOperations = false;
}

void SimFullEntityBinding::simAddOperation(EntityOperationFunc func) {
    std::lock_guard lock(mutex);
    queuedOperations.push(std::move(func));
    hasQueuedOperations = true;
}
