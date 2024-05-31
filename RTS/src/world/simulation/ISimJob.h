#pragma once

class SimTaskHandle;
class ISimTask;
class World;

// Jobs are only created on sim thread
class ISimJob {
public:
    ISimJob() = default;
    ISimJob(World& world, entt::entity jobOwner) : mWorld(world), mJobOwner(jobOwner) {}
    virtual ~ISimJob() = default;

    VORB_NON_COPYABLE(ISimJob);

    virtual std::unique_ptr<ISimTask> tryAquireNextSubtaskForSimCharacter(entt::registry& simRegistry, entt::entity simCharacter) = 0;
    virtual std::unique_ptr<ISimTask> tryAquireNextSubaskForFullCharacter(entt::registry& fullRegistry, entt::entity fullCharacter) = 0;

    virtual void onCompleteTask(ISimTask& task) {};
    virtual void onAbortTask(ISimTask& task) {};

    void addTaskHandle(SimTaskHandle* handle) { mTaskHandles.emplace_back(handle); }
    void removeTaskHandle(SimTaskHandle* handle);
    i32 getRefCount() const { return mTaskHandles.size(); }

    World& getWorld() const { return mWorld; }
protected:
    void finishJob();
    void destroySelf();

    World& mWorld;
    i32 mNumActiveTasks = 0; // A task chain counts as one task
    entt::entity mJobOwner = entt::null;
    bool mFinished = false;
    std::vector<SimTaskHandle*> mTaskHandles; // Task handles held by characters who are using us
};
