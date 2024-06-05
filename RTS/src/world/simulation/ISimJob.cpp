#include "stdafx.h"
#include "ISimJob.h"

#include "world/World.h"
#include "world/simulation/host/SimECS.h"
#include "world/simulation/host/HostSimContext.h"

#include "world/simulation/host/component/SimCharacterComponents.h"

void ISimJob::addTaskHandle(SimTaskHandle* handle) {
    assert(!mFinished);
    std::lock_guard lock(mTaskMutex);
    mTaskHandles.emplace_back(handle);
}

void ISimJob::removeTaskHandle(SimTaskHandle* handle) {
    bool found = false;
    bool canDestroy = false;
    {
        std::lock_guard lock(mTaskMutex);
        for (size_t i = 0; i < mTaskHandles.size(); ++i) {
            if (mTaskHandles[i] == handle) {
                found = true;
                mTaskHandles[i] = mTaskHandles.back();
                mTaskHandles.pop_back();
                canDestroy = mTaskHandles.empty();
                break;
            }
        }
    }
    assert(found);
    if (canDestroy && mFinished) {
        // Will invalidate self
        destroySelf();
    }
}

void ISimJob::finishJob() {
    if (mFinished) return;
    mFinished = true;
    bool canDestroy = false;
    {
        std::lock_guard lock(mTaskMutex);
        canDestroy = mTaskHandles.empty();
    }

    // Only destroy us if all handles are freed
    if (canDestroy) {
        // Will invalidate self
        destroySelf();
    }
}

void ISimJob::destroySelf() {
    auto simThreadCleanupFunc = [this]() {
        entt::registry& registry = mWorld.tryGetSimECS()->getRegistrySimThread();
        SimJobBossComponent& bossCmp = registry.get<SimJobBossComponent>(mJobOwner);
        for (auto&& it = bossCmp.activeJobs.begin(); it != bossCmp.activeJobs.end(); ++it) {
            if (it->get() == this) {
                // This will invalidate ourself!
                if (bossCmp.activeJobs.size() == 1) {
                    // Last job, remove boss component
                    registry.remove<SimJobBossComponent>(mJobOwner);
                }
                else {
                    bossCmp.activeJobs.erase(it);
                }
                return;
            }
        }
    };
    if (IS_SIM_THREAD()) {
        simThreadCleanupFunc();
    }
    else {
        mWorld.tryGetHostSimContext()->addSimThreadTask(std::move(simThreadCleanupFunc));
    }
    panic("Did not find job in boss component");
}
