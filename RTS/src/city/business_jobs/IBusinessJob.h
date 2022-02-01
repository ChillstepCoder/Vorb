#pragma once

#include "ai/tasks/IAgentTask.h"

class IBusinessJob
{
public:
    IBusinessJob() {};
    virtual ~IBusinessJob() = default;
    // Return true when task is done
    virtual bool tick(World & world, entt::registry & registry, entt::entity business) = 0;

    bool canAssignWorker() const { return getNumWorkers() < getMaxWorkers(); }
    virtual void assignWorker(entt::entity worker) = 0;
    
    virtual float getProgress() const = 0;
    bool isDone() const { return getProgress() >= 1.0f; }
    ui32 getMinWorkers() const { return mMinWorkers; }
    ui32 getMaxWorkers() const { return mMaxWorkers; }
    ui32 getNumWorkers() const { return (ui32)mWorkers.size(); }

protected:
    std::vector<entt::entity> mWorkers;
    std::vector<entt::entity> mIdleWorkers;
    ui32 mMinWorkers = 1;
    ui32 mMaxWorkers = 5;
};

