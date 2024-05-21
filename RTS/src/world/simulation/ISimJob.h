#pragma once

class SimTaskHandle;
class ISimTask;
class World;

// Jobs are only created on sim thread
class ISimJob {
public:
    ISimJob() = default;
    ISimJob(entt::entity jobOwner) : mJobOwner(jobOwner) {}
    virtual ~ISimJob() = default;

    virtual std::unique_ptr<ISimTask> tryAquireNextSubtaskForSimCharacter(World& world, entt::registry& simRegistry, entt::entity simCharacter) = 0;
    virtual std::unique_ptr<ISimTask> tryAquireNextSubaskForFullCharacter(World& world, entt::registry& fullRegistry, entt::entity fullCharacter) = 0;

    virtual void onCompleteTask(ISimTask& task) {};
    virtual void onAbortTask(ISimTask& task) {};

    void addTaskHandle(SimTaskHandle* handle) { mTaskHandles.emplace_back(handle); }
    void removeTaskHandle(SimTaskHandle* handle);
    i32 getRefCount() const { return mTaskHandles.size(); }
protected:
    void onFinishedInternal();

    i32 mNumActiveTasks = 0; // A task chain counts as one task
    entt::entity mJobOwner = entt::null;
    std::vector<SimTaskHandle*> mTaskHandles; // Task handles held by characters who are using us
};
