#pragma once

class HostSimContext;
class World;
class SimTaskHandle;
struct SimTaskData;

typedef void(*SimTaskTickFunction)(HostSimContext& context, SimTaskData& task, entt::entity owner, TimestampMs currentTime, bool wasCancelled);

typedef ui32 SimTaskID;
constexpr ui32 INVALID_TASK_ID = UINT32_MAX;

//enum class SimTaskPriority : ui8 {
//    None, // No task
//    Idle,
//    VeryLow,
//    Low,
//    Medium,
//    High,
//    VeryHigh,
//    Critical // Task is life or death
//};

enum class SimTaskType : ui8 {
    Idle,

    COUNT
};
static_assert(e_count(SimTaskType) <= 0xff);

enum class SimTaskFlags : ui8 {
    IsQueued = BIT(0),
};


enum class SimTaskTickResult {
    InProgress,
    Success,
    Fail,
    COUNT
};

class ISimJob;

class ISimTask {
public:
    ISimTask() = default;
    virtual ~ISimTask() = default;
    // Return true when task is done
    virtual void onBeginFull(World& world, entt::registry& fullRegistry, entt::entity fullAgent) {};
    virtual void onBeginSim(World& world, entt::registry& simRegistry, entt::entity simAgent) {};
    virtual SimTaskTickResult tickFull(World& world, entt::registry& fullRegistry, entt::entity fullAgent, f32 elapsedSec) = 0;
    virtual SimTaskTickResult tickSim(World& world, entt::registry& simRegistry, entt::entity simAgent, f32 elapsedSec) = 0;

    // Implemented by ISimTaskChain
    virtual std::unique_ptr<ISimTask>* getNextTask() { return nullptr; }
    virtual void setNextTask(std::unique_ptr<ISimTask>&& nextTask) { panic("Tried to setNextTask on a non ISimTaskChain of type {}", getTaskName()); }

    virtual const char* getTaskName() const = 0;
};

class ISimTaskChain : public ISimTask {
public:
    virtual std::unique_ptr<ISimTask>* getNextTask() override { return &mNextTask; }
    virtual void setNextTask(std::unique_ptr<ISimTask>&& nextTask) { assert(!mNextTask); mNextTask = std::move(nextTask); }
protected:
    std::unique_ptr<ISimTask> mNextTask = nullptr;
};

typedef std::unique_ptr<ISimTask> ISimTaskPtr;
