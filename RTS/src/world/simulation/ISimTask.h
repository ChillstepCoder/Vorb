#pragma once

class HostSimContext;
class World;
struct SimTaskData;

typedef void(*SimTaskTickFunction)(HostSimContext& context, SimTaskData& task, entt::entity owner, TimestampMs currentTime, bool wasCancelled);

typedef ui32 SimTaskID;
constexpr ui32 INVALID_TASK_ID = UINT32_MAX;

enum class SimTaskPriority : ui8 {
    None, // No task
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
    entt::entity taskBoss = entt::null; // If null, we are the boss of this task
};

enum class SimTaskType : ui8 {
    Idle,

    COUNT
};
static_assert(e_count(SimTaskType) <= 0xff);

enum class SimTaskFlags : ui8 {
    IsQueued = BIT(0),
};


enum class SimTaskTickResult {
    IN_PROGRESS,
    SUCCESS,
    FAIL,
    COUNT
};

class ISimJob;

class ISimTask {
public:
    ISimTask() = delete;
    virtual ~ISimTask() = default;
    // Return true when task is done
    virtual SimTaskTickResult tickFull(World& world, entt::registry& fullRegistry, entt::entity fullAgent) = 0;
    virtual SimTaskTickResult tickSim(World& world, entt::registry& simRegistry, entt::entity simAgent) = 0;

    std::unique_ptr<ISimTask>& getNextTask() { return mNextTask; }
    void setNextTask(std::unique_ptr<ISimTask>&& nextTask) { assert(!mNextTask); mNextTask = std::move(nextTask); }

    virtual const char* getTaskName() const = 0;
protected:
    std::unique_ptr<ISimTask> mNextTask = nullptr;
    ISimJob* mParentJob = nullptr; // Optional
    SimTaskPriority mPriority = SimTaskPriority::Idle;
};

typedef std::unique_ptr<ISimTask> ISimTaskPtr;
//
//struct SimTaskData {
//    SimTaskType taskType = SimTaskType::Idle;
//    SimTaskPriority priority = SimTaskPriority::Medium;
//    ui8 currentStep = 0;
//    ui8 lastStep = 0;
//    i32v2 stepStartWorldPos;
//    i32v2 stepEndWorldPos;
//    TimestampMs taskStartTime;
//    TimestampMs lastTickTime;
//    TimestampMs estimatedEndTime;
//    entt::entity taskWorker;
//    bool isFullySimulated = false;
//    BitFlags<SimTaskFlags> flags;
//    ui8 padding[2];
//    SimTaskTickFunction tickFunction = nullptr;
//    void* taskSpecificData = nullptr;
//};
//SIZER(SimTaskData);
