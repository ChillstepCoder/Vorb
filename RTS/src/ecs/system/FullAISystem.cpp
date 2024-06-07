#include "stdafx.h"
#include "FullAISystem.h"


#include "ecs/IEntityComponentSystem.h"
#include "world/World.h"

#include "ecs/component/DualComponents.h"

// TODO: Investigate maslows hierarchy (probability weight?) (Concern probability gradually increases for things that didnt run recently?
// 1. Self Actuation
// 2. Esteem needs
// 3. Belonginess and love needs
// 4. Safety needs
// 5. physiological needs
// https://www.youtube.com/watch?v=RYZSdPuvta8


FullAISystem::FullAISystem(World& world, entt::registry& registry) :
    mWorld(world), mRegistry(registry) {

}

void FullAISystem::update(f32 elapsedSec) {
    PROFILE_FUNCTION();
    auto view = mRegistry.view
        <FullBrainComponent,
         PhysicsComponent,
         DualTaskQueueComponent>();
    for (auto entity : view) {
        updateCharacter(entity);
    }
}

void FullAISystem::updateCharacter(entt::entity entity)
{
    // Tasking
    DualTaskQueueComponent& taskQueue = mRegistry.get<DualTaskQueueComponent>(entity);
    assert(false);
    // Employee stuff

    // Handle sleep schedule

    // Sense danger

    // Needs
}
