#include "stdafx.h"
#include "ISimJob.h"

#include "world/World.h"
#include "world/simulation/host/SimECS.h"

#include "world/simulation/host/component/SimCharacterComponents.h"

void ISimJob::removeTaskHandle(SimTaskHandle* handle) {
    bool found = false;
    for (size_t i = 0; i < mTaskHandles.size(); ++i) {
        if (mTaskHandles[i] == handle) {
            found = true;
            mTaskHandles[i] = mTaskHandles.back();
            mTaskHandles.pop_back();
            break;
        }
    }
    assert(found);
    if (mTaskHandles.empty()) {
        if (mFinished) {
            // Will invalidate self
            destroySelf();
        }
    }
}

void ISimJob::finishJob() {
    if (mFinished) return;
    mFinished = true;

    // Only destroy us if all handles are freed
    if (mTaskHandles.empty()) {
        // Will invalidate self
        destroySelf();
    }
}

void ISimJob::destroySelf() {
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
    panic("Did not find job in boss component");
}
