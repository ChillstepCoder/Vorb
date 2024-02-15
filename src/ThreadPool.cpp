#include "Vorb/stdafx.h"
#include "Vorb/Timing.h"
#include "Vorb/ThreadPool.h"
#include "Vorb/logging/Logger.h"

vorb::core::ThreadPool::ThreadPool(ui32 size) {
    /// Allocate all threads
    mWorkers.resize(size);
    for (ui32 i = 0; i < size; i++) {
        mWorkers[i] = std::make_unique<WorkerThread>(&ThreadPool::workerThreadFunc, this);
        ++mActiveThreads;
    }
}


vcore::ThreadPool::~ThreadPool() {
    // Clear out the queue
    clearTasks();

    // Tell all threads to wake up and close, as they are currently hanging on a semaphore
    for (size_t i = 0; i < mWorkers.size(); i++) {
        mWorkers[i]->mStop.store(true);
    }
    for (size_t i = 0; i < mWorkers.size(); i++) {
        addTask(nullptr);
        --mActiveThreads;
    }

    // Join all threads
    for (size_t i = 0; i < mWorkers.size(); i++) {
        mWorkers[i]->join();
    }
}

void vcore::ThreadPool::clearTasks() {
    // Dequeue all tasks
    std::function<void()> task[256];
    while (mTasks.try_dequeue_bulk(task, 256));
}

void vcore::ThreadPool::workerThreadFunc(WorkerThread* thisThread) {
    std::function<void()> task;
    while (!thisThread->mStop.load()) {
        // Note that threads will be stuck waiting here until the process ends
        mTasks.wait_dequeue(task);
        ++mRunningThreads;
        // No task pointer means the thread should stop
        if (!task) {
            --mRunningThreads;
            return;
        }
        task();
        --mRunningThreads;
    }
    thisThread->mActive = false;
}

void vorb::core::ThreadPool::setSize(ui32 size) {
    const i32 diff = size - mActiveThreads;
    if (diff < 0) {
        for (ui32 i = 0; i < (ui32)(-diff); ++i) {
            --mActiveThreads;
            addTask(nullptr);
        }
        return;
    }
    else if (diff > 0) {
        for (ui32 i = 0; i < diff; ++i) {
            ++mActiveThreads;
            mWorkers.emplace_back(std::make_unique<WorkerThread>(&ThreadPool::workerThreadFunc, this));
        }
    }

    // Clear any finished threads (not important)
    for (size_t i = 0; i < mWorkers.size();) {
        if (!mWorkers[i]->mActive) {
            mWorkers[i]->join();
            mWorkers[i] = std::move(mWorkers.back());
            mWorkers.pop_back();
        }
        else {
            ++i;
        }
    }
}