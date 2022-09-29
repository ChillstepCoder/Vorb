#include "stdafx.h"
#include "PersonAIComponent.h"
#include "PhysicsComponent.h"

#include "ecs/EntityComponentSystem.h"

#include "world/IWorld.h"
#include "city/City.h"
#include "city/CityBusinessManager.h"

#include "ecs/component/EmployeeComponent.h"

PersonAISystem::PersonAISystem()
{
}

// TODO: Refactor
inline void updateComponent(entt::registry& registry, entt::entity entity, PersonAIComponent& ai, PhysicsComponent& physics) {
    
    // Set home to first city if none (TODO: better residence)
    if (!ai.mCity) {
        ai.mCity = sWorld->getCityGraph().getClosestCityToPoint(physics.getPosition());
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
        // TODO: Don't repeatedly try every update
        if (employeeCmp) {
            employeeCmp->flags = 0;
            employeeCmp->mCurrentTask = nullptr;
        }
    }

    // Select which task to do
    // TODO: OnInterrupt for each task, to evaluate if we should interrupt based on external changes
    if (employeeCmp) {
        if (employeeCmp->mCurrentTask) {
            if (employeeCmp->mCurrentTask->tick(registry, entity)) {
                IAgentTaskPtr& nextTask = employeeCmp->mCurrentTask->getNextTask();
                // This will free old task shared_ptr
                employeeCmp->mCurrentTask = std::move(nextTask);
                nextTask.reset();
            }
        }
        else {
            // If we don't have a task, grab one
            if ((employeeCmp->flags & EmployeeComponentFlags::FLAG_EMPLOYEE_IS_IDLE) == 0) {
                BusinessComponent& businessCmp = registry.get<BusinessComponent>(employeeCmp->mBusiness);
                businessCmp.addIdleWorker(entity);
                employeeCmp->flags |= EmployeeComponentFlags::FLAG_EMPLOYEE_IS_IDLE;
            }
        }
    }

    // Handle sleep schedule

    // Sense danger

    // Needs
}

void PersonAISystem::update(entt::registry& registry) {
    auto view = registry.view<PersonAIComponent, PhysicsComponent>();
    for (auto entity : view) {
        PersonAIComponent& ai = view.get<PersonAIComponent>(entity);
        PhysicsComponent& physics = view.get<PhysicsComponent>(entity);
        updateComponent(registry, entity, ai, physics);
    }
}