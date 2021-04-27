#include "stdafx.h"
#include "PersonAIComponent.h"
#include "PhysicsComponent.h"

#include "ecs/EntityComponentSystem.h"

#include "World.h"
#include "city/City.h"
#include "services/Services.h"

PersonAISystem::PersonAISystem(World& world)
    : mWorld(world)
{
}

struct ChopWoodTaskData {
    Path path;
};

void updateChopWoodTask(World& world, PersonAIComponent& ai, PhysicsComponent& physics, float deltaTime) {
    // Begin task
    if (!ai.mCurrentTaskData) {
        ChopWoodTaskData* taskData = new ChopWoodTaskData;
        ai.mCurrentTaskData = taskData;
        taskData->path = Services::PathFinder::ref().generatePathSynchronous(world, physics.getXYPosition(), ???)
    }
}

// TODO: Refactor
inline void updateComponent(World& world, entt::entity entity, PersonAIComponent& ai, PhysicsComponent& physics, float deltaTime) {
    
    // Set home to first city if none (TODO: better residence)
    if (!ai.mCity) {
        ai.mCity = world.getClosestCityToPoint(physics.getXYPosition());
        // No city? No work!
        if (!ai.mCity) {
            return;
        }
        // Notify city of our residence
        ai.mCity->addResidentToCity(entity);
    }


    switch (ai.currentTask) {
        case PersonAITask::IDLE: {
            // Ask city for work
            ai.currentTask = PersonAITask::CHOP_WOOD;
            break;
        }
        case PersonAITask::CHOP_WOOD: {
            updateChopWoodTask(world, ai, physics, deltaTime);
            break;
        }
        case PersonAITask::BUILD:
            break;
        case PersonAITask::SLEEP:
            break;
        default:
            assert(false);
            break;
    }
}

void PersonAISystem::update(entt::registry& registry, float deltaTime)
{
    auto view = registry.view<PersonAIComponent, PhysicsComponent>();
    for (auto entity : view) {
        PersonAIComponent& ai = view.get<PersonAIComponent>(entity);
        PhysicsComponent& physics = view.get<PhysicsComponent>(entity);
        updateComponent(mWorld, entity, ai, physics, deltaTime);
    }
}