#pragma once

class HostSimContext;
struct SimTaskData;

typedef void(*SimTaskTickFunction)(HostSimContext& context, SimTaskData& task, entt::entity owner, SimTimestamp currentTime, bool wasCancelled);

typedef ui32 SimTaskID;
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

struct SimTaskHandle {
    SimTaskID taskId = INVALID_TASK_ID;
    entt::entity taskBoss = entt::null; // Can be ourselves or someone else
};

enum class SimTaskType : ui8 {
    Idle,

    COUNT
};
static_assert(e_count(SimTaskType) <= 0xff);

enum class SimTaskFlags : ui8 {
    IsQueued = BIT(0),
};

struct SimTaskData {
    SimTaskType taskType = SimTaskType::Idle;
    SimTaskPriority priority = SimTaskPriority::Medium;
    ui8 currentStep = 0;
    ui8 lastStep = 0;
    i32v2 stepStartWorldPos;
    i32v2 stepEndWorldPos;
    SimTimestamp taskStartTime;
    SimTimestamp lastTickTime;
    SimTimestamp estimatedEndTime;
    entt::entity taskWorker;
    bool isFullySimulated = false;
    BitFlags<SimTaskFlags> flags;
    ui8 padding[2];
    SimTaskTickFunction tickFunction = nullptr;
    void* taskSpecificData = nullptr;
};
//SIZER(SimTaskData);

// Represents a discrete action with both a simulated and full implementation
class SimTaskStep {

};

class SimTaskGatherResource {
public:
    static void beginTask(HostSimContext& context, SimTaskData& task, entt::entity owner);
protected:
    static SimTaskTickFunction getTickFunc();
};