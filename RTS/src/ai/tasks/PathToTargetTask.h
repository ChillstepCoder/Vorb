#pragma once

#include "IAgentTask.h"
#include "tile/TileHandle.h"
#include "city/contracts/ItemShipmentContract.h"

class PathToTargetTask : public IAgentTask
{
public:
    PathToTargetTask(TileHandle targetPosition, f32 completionRadius, AgentTaskFinishedFunc finishedFunc);
    ~PathToTargetTask();

    // Override allocation to use boost::singleton_pool
    static void* operator new(size_t count);
    static void operator delete(void* pointer, size_t size);

    TaskTickResult tick(World& world, entt::registry& registry, entt::entity agent) override;
    const char* getTaskName() const override { return "PathToTarget"; }

protected:
    enum class TaskState : ui8 {
        NEEDS_PATH,
        PATHING,
        FAIL,
        SUCCESS
    };

    TileRef mTargetHandle; // TODO: Not ref? Allow unload?
    f32v3 mTargetPosition;
    f32 mCompletionRadiusSQ;
    TaskState mState = TaskState::NEEDS_PATH;
};

typedef std::unique_ptr<PathToTargetTask> PathToTargetTaskPtr;