#pragma once

class World;

// Represents a job for a person, lists of AgentTasks can form todo lists or schedules
class IAgentTask {
public:
    IAgentTask() {};
    virtual ~IAgentTask() = default;
    // Return true when task is done
    virtual bool tick(World& world, entt::registry& registry, entt::entity agent) = 0;

    std::unique_ptr<IAgentTask>& getNextTask() { return mNextTask; }

private:
    std::unique_ptr<IAgentTask> mNextTask = nullptr;
};

typedef std::unique_ptr<IAgentTask> IAgentTaskPtr;
