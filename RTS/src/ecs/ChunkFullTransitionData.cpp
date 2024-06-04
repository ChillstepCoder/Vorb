#include "stdafx.h"
#include "ChunkFullTransitionData.h"

#include "ecs/component/Components.h"
#include "ecs/component/FullEntityBindingComponent.h"

POOLED_ALLOC_DEF_THREADSAFE(EntityComponentCharacterTransitionData, 256);

void EntityComponentCharacterTransitionData::moveFromEntity(entt::registry& registry, entt::entity entity)
{
    assert(!isValid);
    isValid = true;
    character = std::move(registry.get<SimCharacterComponent>(entity));
    registry.remove<SimCharacterComponent>(entity);
    taskQueue = std::move(registry.get<SimTaskQueueComponent>(entity));
    registry.remove<SimTaskQueueComponent>(entity);
    attributes = std::move(registry.get<AttributesComponent>(entity));
    registry.remove<AttributesComponent>(entity);
    inventory = std::move(registry.get<InventoryComponent>(entity));
    registry.remove<InventoryComponent>(entity);
    gender = std::move(registry.get<SimGenderComponent>(entity));
    registry.remove<SimGenderComponent>(entity);
}

void EntityComponentCharacterTransitionData::moveToEntity(entt::registry& registry, entt::entity entity)
{
    assert(isValid);
    isValid = false;
    registry.emplace<SimCharacterComponent>(entity, std::move(character));
    registry.emplace<SimTaskQueueComponent>(entity, std::move(taskQueue));
    registry.emplace<AttributesComponent>(entity, std::move(attributes));
    registry.emplace<InventoryComponent>(entity, std::move(inventory));
    registry.emplace<SimGenderComponent>(entity, std::move(gender));
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
