#pragma once

#include "ai/tasks/IAgentTask.h"

class IBusinessJob
{
public:
    IBusinessJob() {};
    virtual ~IBusinessJob() = default;

    // ============= Abstract interface =============
    // Return true when task is done
    virtual bool tick(entt::registry & registry, entt::entity business) = 0;
    virtual float getProgress() const = 0;
    virtual IAgentTaskPtr tryMakeTaskForWorker(entt::entity worker) = 0;

    // ============= Task interface =============
    bool isDone() const { return getProgress() >= 1.0f; }

protected:

};

