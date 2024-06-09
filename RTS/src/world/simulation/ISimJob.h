#pragma once

class SimTaskHandle;
class ISimTask;
class World;

// Jobs can be shared between threads
class ISimJob {
public:
    ISimJob() = default;
    ISimJob(World& world, entt::entity jobOwner) : mWorld(world), mJobOwner(jobOwner) {}
    virtual ~ISimJob() = default;

    VORB_NON_COPYABLE(ISimJob);

    virtual std::unique_ptr<ISimTask> tryAquireNextSubtaskForSimCharacter(entt::registry& simRegistry, entt::entity simCharacter) = 0;
    virtual std::unique_ptr<ISimTask> tryAquireNextSubtaskForFullCharacter(entt::registry& fullRegistry, entt::entity fullCharacter) = 0;

    virtual void onCompleteTask(ISimTask& task) {};
    virtual void onAbortTask(ISimTask& task) {};

    void addTaskHandle(SimTaskHandle* handle);
    void removeTaskHandle(SimTaskHandle* handle);
    i32 getRefCount() const { return mTaskHandles.size(); }

    bool isFinished() const { return mFinished; }
    World& getWorld() const { return mWorld; }
protected:
    void finishJob();
    void destroySelf();

    World& mWorld;
    entt::entity mJobOwner = entt::null;
    std::atomic_bool mFinished = false;
    std::mutex mTaskMutex;
    std::vector<SimTaskHandle*> mTaskHandles; // Task handles held by characters who are using us
};
