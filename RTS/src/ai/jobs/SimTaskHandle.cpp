#include "stdafx.h"
#include "SimTaskHandle.h"

#include "world/simulation/ISimJob.h"

POOLED_ALLOC_DEF_NOT_THREADSAFE(SimTaskHandle, 256, ASSERT_SIM_THREAD());

SimTaskHandle::SimTaskHandle(ISimJob* job, entt::entity owner) : mJob(job), mIsJob(true), mOwner(owner) {
    mJob->addTaskHandle(this);
}

SimTaskHandle::SimTaskHandle(SimTaskHandle&& other) noexcept {
    ASSERT_SIM_THREAD();
    // Bitwise copy
    memcpy(this, &other, sizeof(SimTaskHandle));

    // Disables cleanup
    other.mIsJob = false;
}

SimTaskHandle& SimTaskHandle::operator=(SimTaskHandle&& other) noexcept {
    ASSERT_SIM_THREAD();
    // Bitwise copy
    memcpy(this, &other, sizeof(SimTaskHandle));

    // Disables cleanup
    other.mIsJob = false;
    return *this;
}

SimTaskHandle::~SimTaskHandle() {
    ASSERT_SIM_THREAD();
    if (mIsJob) {
        mJob->removeTaskHandle(this);
    }
}
