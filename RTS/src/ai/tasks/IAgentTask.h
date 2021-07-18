#pragma once

class World;

// Represents a job for a person, lists of AgentTasks can form todo lists or schedules
class IAgentTask {
public:
    IAgentTask() {};
    virtual ~IAgentTask() = default;
    // Return true when task is done
    virtual bool tick(World& world, entt::registry& registry, entt::entity agent) = 0;

    std::shared_ptr<IAgentTask> getNextTask() { return mNextTask; }

private:
    std::shared_ptr<IAgentTask> mNextTask = nullptr;
};

typedef std::shared_ptr<IAgentTask> IAgentTaskPtr; // RAW POINTER
