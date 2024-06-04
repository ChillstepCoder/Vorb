#include "stdafx.h"
#include "SimTaskHandle.h"

#include "world/simulation/ISimJob.h"
#include "world/simulation/ISimTask.h"

POOLED_ALLOC_DEF_NOT_THREADSAFE(SimTaskHandle, 256, ASSERT_SIM_THREAD());

SimTaskHandle::SimTaskHandle(World& world, entt::registry& simRegistry, std::unique_ptr<ISimTask>&& task) : mTask(std::move(task)), mIsJob(false) {
    ASSERT_SIM_THREAD();
}

SimTaskHandle::SimTaskHandle(ISimJob* job) : mJob(job), mIsJob(true) {

}

void SimTaskHandle::init() {
    // This cannot be done in the constructor, as the object may have been moved
    mDidInit = true;
    if (mIsJob) {
        mJob->addTaskHandle(this);
    }
}

SimTaskHandle::~SimTaskHandle() {
    if (!IS_SHUTTING_DOWN) {
        ASSERT_SIM_THREAD();
    }

    if (mIsJob) {
        // If this crashes we probably forgot to call init()
        mJob->removeTaskHandle(this);
    }
}

SimTaskHandle::SimTaskHandle(SimTaskHandle&& other) noexcept {
    assert(!other.mDidInit); // We cannot move an already initialized handle as we require a stable pointer in the parent job
    ASSERT_SIM_THREAD();
    // Bitwise copy
    memcpy(this, &other, sizeof(SimTaskHandle));

    // Disables cleanup
    other.mIsJob = false;
}


SimTaskHandle& SimTaskHandle::operator=(SimTaskHandle&& other) noexcept {
    assert(!other.mDidInit); // We cannot move an already initialized handle as we require a stable pointer in the parent job
    ASSERT_SIM_THREAD();
    // Bitwise copy
    memcpy(this, &other, sizeof(SimTaskHandle));

    // Disables cleanup
    other.mIsJob = false;
    return *this;
}


ISimTask* SimTaskHandle::getOrAquireActiveTaskForSimCharacter(World& world, entt::registry& simRegistry, entt::entity simCharacter) {
    ASSERT_SIM_THREAD();
    assert(mDidInit);
    if (mIsJob) {
        if (mTask) {
            return mTask.get();
        }
        mTask = mJob->tryAquireNextSubtaskForSimCharacter(simRegistry, simCharacter);
        if (mTask) {
            return mTask.get();
        }
        // Signals that we are done with the job
        return nullptr;
    }
    else {
        return mTask.get();
    }
}

ISimTask* SimTaskHandle::getOrAquireActiveTaskForFullCharacter(World& world, entt::registry& fullRegistry, entt::entity fullCharacter) {
    ASSERT_GAME_THREAD();
    assert(mDidInit);
    panic("getOrAquireActiveTaskForFullCharacter NOT IMPLEMENTED");
}

bool SimTaskHandle::isFinished() {
    if (mIsJob) {
        return mJob->isFinished();
    }
    else {
        return mTask == nullptr;
    }
}

void SimTaskHandle::onActiveSubtaskGoToNextTask(ISimTask* task) {
    assert(task == mTask.get());
    assert(task->getNextTask());
    // Making a copy just in case destructor runs first on activeJobSubtask copy
    auto subtaskCopy = std::move(task->getNextTask());
    mTask = std::move(*subtaskCopy);
}

void SimTaskHandle::onActiveSubtaskFinished(ISimTask* task) {
    assert(!task->getNextTask());
    assert(task == mTask.get());

    if (mIsJob) {
        mJob->onCompleteTask(*task);
    }

    mTask.reset();
}

void SimTaskHandle::onActiveSubtaskAborted(ISimTask* task) {
    assert(task == mTask.get());

    if (mIsJob) {
        mJob->onAbortTask(*task);
    }

    mTask.reset();
}
