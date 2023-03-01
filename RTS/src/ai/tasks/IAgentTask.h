#pragma once

// Represents a job for a person, lists of AgentTasks can form todo lists or schedules
class IAgentTask {
public:
    IAgentTask() {};
    virtual ~IAgentTask() = default;
    // Return true when task is done
    virtual bool tick(entt::registry& registry, entt::entity agent) = 0;

    std::unique_ptr<IAgentTask>& getNextTask() { return mNextTask; }
    void setNextTask(std::unique_ptr<IAgentTask>&& nextTask) { assert(!mNextTask); mNextTask = std::move(nextTask); }

private:
    std::unique_ptr<IAgentTask> mNextTask = nullptr;
};

typedef std::unique_ptr<IAgentTask> IAgentTaskPtr;
