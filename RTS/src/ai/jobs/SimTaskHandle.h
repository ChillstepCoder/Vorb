#pragma once

class ISimTask;
class ISimJob;
class World;

enum class SimTaskStatus {
    Idle,
    Running,
    Finished,
    COUNT
};

class SimTaskHandle {
    friend class SimAISystem; // TODO: Interface wrapper to expose only the necessary functionality
public:
    SimTaskHandle(World& world, entt::registry& simRegistry, std::unique_ptr<ISimTask>&& task, entt::entity owner);
    SimTaskHandle(ISimJob* job, entt::entity owner);
    ~SimTaskHandle();

    SimTaskHandle(SimTaskHandle&& other) noexcept;
    SimTaskHandle& operator=(SimTaskHandle&& other) noexcept;

    void init();

    VORB_NON_COPYABLE(SimTaskHandle);

    POOLED_ALLOC_DECL();

    // Will either return the task, or will return a task from the job, if possible
    ISimTask* getOrAquireActiveTaskForSimCharacter(World& world, entt::registry& simRegistry, entt::entity simCharacter);
    ISimTask* getOrAquireActiveTaskForFullCharacter(World& world, entt::registry& fullRegistry, entt::entity fullCharacter);

    bool isFinished();

    bool isJob() const { return mIsJob; }
    ISimJob* getJob() const { assert(mIsJob); return mJob; }
    ISimTask* getTask() const { assert(!mIsJob); return mTask.get(); }
    i16 getPriority() const { return mPriority; }
    void setPriority(i16 p) { mPriority = p; }

private:

    void onActiveSubtaskGoToNextTask(ISimTask* task);
    void onActiveSubtaskFinished(ISimTask* task);
    void onActiveSubtaskAborted(ISimTask* task);

    ISimJob* mJob = nullptr;
    std::unique_ptr<ISimTask> mTask;
    i16 mPriority = 10;
    bool mIsJob = false;
    bool mDidInit = false;
    entt::entity mOwner;
};
