#include "stdafx.h"
#include "SimTaskHandle.h"

#include "world/simulation/ISimJob.h"
#include "world/simulation/ISimTask.h"

POOLED_ALLOC_DEF_NOT_THREADSAFE(SimTaskHandle, 256, ASSERT_SIM_THREAD());

SimTaskHandle::SimTaskHandle(World& world, entt::registry& simRegistry, ISimTask* task, entt::entity owner) : mTask(task), mIsJob(false), mOwner(owner) {
    ASSERT_SIM_THREAD();
    mTask->onBeginSim(world, simRegistry, owner);
}

SimTaskHandle::SimTaskHandle(ISimJob* job, entt::entity owner) : mJob(job), mIsJob(true), mOwner(owner) {
    mJob->addTaskHandle(this);
}

SimTaskHandle::~SimTaskHandle() {
    ASSERT_SIM_THREAD();
    if (mIsJob) {
        mJob->removeTaskHandle(this);
    }
}

SimTaskHandle::SimTaskHandle(SimTaskHandle&& other) noexcept {
    ASSERT_SIM_THREAD();
    // Bitwise copy
    memcpy(this, &other, sizeof(SimTaskHandle));

    // Disables cleanup
    other.mIsJob = false;
}

ISimTask* SimTaskHandle::getOrAquireActiveTaskForSimCharacter(World& world, entt::registry& simRegistry, entt::entity simCharacter) {
    ASSERT_SIM_THREAD();
    if (mIsJob) {
        if (mActiveJobSubtask) {
            return mActiveJobSubtask.get();
        }
        mActiveJobSubtask = mJob->tryAquireNextSubtaskForSimCharacter(world, simRegistry, simCharacter);
        if (mActiveJobSubtask) {
            mActiveJobSubtask->onBeginSim(world, simRegistry, simCharacter);
            return mActiveJobSubtask.get();
        }
        // Signals that we are done with the job
        return nullptr;
    }
    else {
        return mTask;
    }
}

ISimTask* SimTaskHandle::getOrAquireActiveTaskForFullCharacter(World& world, entt::registry& fullRegistry, entt::entity fullCharacter) {
    ASSERT_GAME_THREAD();
    panic("getOrAquireActiveTaskForFullCharacter NOT IMPLEMENTED");
}


void SimTaskHandle::onActiveSubtaskGoToNextTask(ISimTask* task) {
    assert(task == mActiveJobSubtask.get());
    assert(task->getNextTask());
    // Making a copy just in case destructor runs first on activeJobSubtask copy
    auto subtaskCopy = std::move(task->getNextTask());
    mActiveJobSubtask = std::move(*subtaskCopy);
}

void SimTaskHandle::onActiveSubtaskFinished(ISimTask* task) {
    assert(!task->getNextTask());

    if (mIsJob) {
        assert(task == mActiveJobSubtask.get());
        mJob->onCompleteTask(*task);
        mActiveJobSubtask.reset();
    }
    else {
        assert(task == mTask);
        mTask = nullptr;
        assert(false); // This feels wrong, wheres the ownership of the task object?? Should we just use mActiveJobSubtask?
    }
}

void SimTaskHandle::onActiveSubtaskAborted(ISimTask* task) {
    if (mIsJob) {
        assert(task == mActiveJobSubtask.get());
        mJob->onAbortTask(*task);
        mActiveJobSubtask.reset();
    }
    else {
        mTask = nullptr;
    }
}

SimTaskHandle& SimTaskHandle::operator=(SimTaskHandle&& other) noexcept {
    ASSERT_SIM_THREAD();
    // Bitwise copy
    memcpy(this, &other, sizeof(SimTaskHandle));

    // Disables cleanup
    other.mIsJob = false;
    return *this;
}
