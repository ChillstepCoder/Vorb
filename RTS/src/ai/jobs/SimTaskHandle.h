#pragma once

class ISimTask;
class ISimJob;

class SimTaskHandle {
public:
    SimTaskHandle(ISimTask* task, entt::entity owner) : mTask(task), mIsJob(false), mOwner(owner) {}
    SimTaskHandle(ISimJob* job, entt::entity owner);
    ~SimTaskHandle();

    SimTaskHandle(SimTaskHandle&& other) noexcept;
    SimTaskHandle& operator=(SimTaskHandle&& other) noexcept;

    POOLED_ALLOC_DECL();

    bool isJob() const { return isJob; }
    ISimJob* getJob() const { assert(isJob); return mJob; }
    ISimTask* getTask() const { assert(!isJob); return mTask; }
    i16 getPriority() const { return mPriority; }
    void setPriority(i16 p) { mPriority = p; }

private:
    union {
        ISimTask* mTask;
        ISimJob* mJob;
    };
    i16 mPriority = 10;
    bool mIsJob = false;
    entt::entity mOwner;
};