#include "stdafx.h"
#include "PersonAIComponent.h"
#include "PhysicsComponent.h"

#include "ecs/EntityComponentSystem.h"

#include "World.h"
#include "city/City.h"
#include "city/CityBusinessManager.h"

#include "services/Services.h"

#include "ecs/component/EmployeeComponent.h"

PersonAISystem::PersonAISystem(World& world)
    : mWorld(world)
{
}

// TODO: Refactor
inline void updateComponent(World& world, entt::registry& registry, entt::entity entity, PersonAIComponent& ai, PhysicsComponent& physics) {
    
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

    // Pick our employment if we dont have one
    // TODO: Make this smarter, more organic. Use jobs board
    EmployeeComponent* employeeCmp = registry.try_get<EmployeeComponent>(entity);
    if (!employeeCmp) {
        ai.mCity->getBusinessManager().tryEmploy(entity);
        employeeCmp = registry.try_get<EmployeeComponent>(entity); // Could still be null
    }

    // Select which task to do
    // TODO: OnInterrupt for each task, to evaluate if we should interrupt based on external changes

    if (ai.mCurrentTask) {
        if (ai.mCurrentTask->tick(world, registry, entity)) {
            IAgentTaskPtr nextTask = ai.mCurrentTask->getNextTask();
            // This can set it to nullptr
            ai.mCurrentTask = nextTask;
        }
    } else if (employeeCmp) {
        // If we don't have a task, grab one
        BusinessComponent& businessCmp = registry.get<BusinessComponent>(employeeCmp->mBusiness);
        ai.mCurrentTask = businessCmp.aquireTask();
    }

    // Handle sleep schedule

    // Sense danger

    //switch (ai.currentTask) {
    //    case PersonAITask::IDLE: {
    //        // Ask city for work
    //        ai.currentTask = PersonAITask::CHOP_WOOD;
    //        break;
    //    }
    //    case PersonAITask::CHOP_WOOD: {
    //        // TODO: Change this instead to a task VirtualFunction
    //        // Task chains
    //        updateChopWoodTask(world, ai, physics);
    //        break;
    //    }
    //    case PersonAITask::BUILD:
    //        break;
    //    case PersonAITask::SLEEP:
    //        break;
    //    default:
    //        assert(false);
    //        break;
    //}
}

void PersonAISystem::update(entt::registry& registry)
{
    auto view = registry.view<PersonAIComponent, PhysicsComponent>();
    for (auto entity : view) {
        PersonAIComponent& ai = view.get<PersonAIComponent>(entity);
        PhysicsComponent& physics = view.get<PhysicsComponent>(entity);
        updateComponent(mWorld, registry, entity, ai, physics);
    }
}