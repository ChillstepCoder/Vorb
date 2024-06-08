#include "stdafx.h"
#include "FullAISystem.h"


#include "ecs/IFullECS.h"
#include "world/World.h"

#include "ecs/component/DualComponents.h"

#include "ai/jobs/SimTaskHandle.h"

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
    mElapsedSec = elapsedSec;

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
   
    if (taskQueue.taskQueue.size()) {
        updateTask(entity, taskQueue);
    }

    // Employee stuff

    // Handle sleep schedule

    // Sense danger

    // Needs
}

void FullAISystem::updateTask(entt::entity entity, DualTaskQueueComponent& taskCmp) {
    assert(taskCmp.taskQueue.size());
    // Acquire task if needed
    if (!taskCmp.activeTask) {
        for (size_t i = 0; i < taskCmp.taskQueue.size();) {
            taskCmp.activeTask = taskCmp.taskQueue[i].getOrAquireActiveTaskForFullCharacter(mWorld, mRegistry, entity);
            if (taskCmp.activeTask) {
                break;
            }
            else {
                if (taskCmp.taskQueue[i].isFinished()) {
                    if (i == 0) {
                        taskCmp.taskQueue.pop_front();
                    }
                    else {
                        taskCmp.taskQueue.erase(taskCmp.taskQueue.begin() + i);
                    }
                }
                else {
                    ++i;
                }
            }
        }

        if (!taskCmp.activeTask) {
            return;
        }
    }

    // Operate on task
    SimTaskTickResult tickResult = taskCmp.activeTask->tickSim(mWorld, mRegistry, entity, mElapsedSec);
    if (tickResult != SimTaskTickResult::InProgress) {
        if (tickResult == SimTaskTickResult::Success) {
            taskCmp.taskQueue.front().onActiveSubtaskFinished(taskCmp.activeTask);
        }
        else {
            taskCmp.taskQueue.front().onActiveSubtaskAborted(taskCmp.activeTask);
        }
        // We will grab a new task next tick, and potentially be fully done with this TaskHandle if there
        // are none left
        taskCmp.activeTask = nullptr;
        assert(e_count(SimTaskTickResult) == 3);
    }
}
