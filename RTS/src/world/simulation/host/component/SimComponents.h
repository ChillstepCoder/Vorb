#pragma once

#include <boost/container/flat_map.hpp>

class HostSimContext;

typedef void(*SimTaskFinishedFunction)(HostSimContext& context, SimTask& task, entt::entity owner);

// Owned by the Simulation Thread ECS

// Owned by the Simulation Thread ECS
struct SimPositionComponent {
    f32v2 position; // Usually "last known" position
};

struct SimCharacterComponent {
    CharacterUID mCharacterId;
};

typedef ui32 TaskID;
constexpr ui32 INVALID_TASK_ID = UINT32_MAX;

enum class SimTaskPriority : ui8 {
    Idle,
    VeryLow,
    Low,  
    Medium,
    High,
    VeryHigh,
    Critical // Task is life or death
};

struct TaskHandle {
    TaskID taskId = INVALID_TASK_ID;
    entt::entity taskBoss = entt::null; // Can be ourselves or someone else
};

// Extemely simple decision maker as we will be running tens of thousands of these on the
// sim thread
struct SimBrainComponent {
    TimestampCentisec taskStartTime;
    TimestampCentisec taskEndTime;
    TaskHandle currentTask;
    SimTaskPriority taskPriority = SimTaskPriority::Idle;
    ui8 padding[3];
};
static_assert(sizeof(SimBrainComponent) == 20, "Keep small for cache efficiency");

struct SimDetailsComponent {
    f32 mHunger = 0.0f; // [0, 1>, 1 is starving. Can go beyond 1.
    f32 mHealth = 1.0f; // [0, 1], 1 is healthy. 0 is dead.
    f32 mFun = 0.0f; // [0, 1], 1 is fully satisfied. 
};

struct SimTask {
    SimTaskPriority priority;
    TimestampCentisec taskStartTime;
    TimestampCentisec taskEndTime;
    SimTaskFinishedFunction endFunction = nullptr;
    entt::entity taskWorker;
};

struct SimTaskQueueComponent {
    std::vector<SimTask> mTaskQueue; // TODO: Dequeue? Something better? PriorityQueue?
};

// Represents a list of tasks, and who is working on them for us. Does not include tasks we are doing for ourselves
struct SimTaskBossComponent {
    boost::flat_map<TaskID, SimTask> mActiveTasks;

};