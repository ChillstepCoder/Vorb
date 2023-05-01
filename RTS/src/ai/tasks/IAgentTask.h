#pragma once

enum class TaskTickResult {
    IN_PROGRESS,
    SUCCESS,
    FAIL,
    COUNT
};

class IAgentTask;
class IWorld;

typedef bool (*AgentTaskFinishedFunc)(bool, IAgentTask*);

// Represents a job for a person, lists of AgentTasks can form todo lists or schedules
class IAgentTask {
public:
    IAgentTask() = delete;
    IAgentTask(AgentTaskFinishedFunc func) : mTaskFinishedFunc(func) {};
    virtual ~IAgentTask() = default;
    // Return true when task is done
    virtual TaskTickResult tick(IWorld& world, entt::registry& registry, entt::entity agent) = 0;

    std::unique_ptr<IAgentTask>& getNextTask() { return mNextTask; }
    void setNextTask(std::unique_ptr<IAgentTask>&& nextTask) { assert(!mNextTask); mNextTask = std::move(nextTask); }

    virtual const char* getTaskName() const = 0;

    AgentTaskFinishedFunc getFinishedFunc() const { return mTaskFinishedFunc; }

private:
    std::unique_ptr<IAgentTask> mNextTask = nullptr;
    AgentTaskFinishedFunc mTaskFinishedFunc = nullptr;
};

typedef std::unique_ptr<IAgentTask> IAgentTaskPtr;
