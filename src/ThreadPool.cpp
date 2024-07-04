#include "Vorb/stdafx.h"
#include "Vorb/Timing.h"
#include "Vorb/ThreadPool.h"
#include "Vorb/logging/Logger.h"

#include <format>

static std::atomic_int WORKER_THREAD_ID_COUNTER = 0;

void setWorkerThreadName() {
    const int id = WORKER_THREAD_ID_COUNTER.fetch_add(1);
    std::string name = std::format("Worker {}", id);
    wchar_t wc[64];
    size_t outSize;
    mbstowcs_s(&outSize, wc, name.size() + 1, name.c_str(), 64);
    SetThreadDescription(GetCurrentThread(), wc);
}

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
        addTask([]{}, TaskPriority::High);
        --mActiveThreads;
    }

    // Join all threads
    for (size_t i = 0; i < mWorkers.size(); i++) {
        mWorkers[i]->join();
    }
    for (size_t i = 0; i < mDeadWorkers.size(); i++) {
        mDeadWorkers[i]->join();
    }
}

void vcore::ThreadPool::clearTasks() {
    // Dequeue all tasks
    std::function<void()> task[256];
    for (int i = 0; i < (int)TaskPriority::COUNT; i++) {
        while (mTasks[i].try_dequeue_bulk(task, 256));
    }
}

void vcore::ThreadPool::workerThreadFunc(WorkerThread* thisThread) {
    std::function<void()> task;

    setWorkerThreadName();

    moodycamel::ConsumerToken ctok[(int)TaskPriority::COUNT] = {
        moodycamel::ConsumerToken(mTasks[0]),
        moodycamel::ConsumerToken(mTasks[1]),
        moodycamel::ConsumerToken(mTasks[2])
    };
    while (!thisThread->mStop.load()) {
        mTaskSemaphore.acquire();
        ++mRunningThreads;
        if (mTasks[(int)TaskPriority::High].try_dequeue(ctok[(int)TaskPriority::High], task)) {
            task();
        }
        else if (mTasks[(int)TaskPriority::Normal].try_dequeue(ctok[(int)TaskPriority::Normal], task)) {
            task();
        }
        else if (mTasks[(int)TaskPriority::Low].try_dequeue(ctok[(int)TaskPriority::Low], task)) {
            task();
        }
        else if (!thisThread->mStop.load()) {
            // Rare case, someone grabbed our task as we were in the middle of checking,
            // which means we should be grabbing a higher priority one.
            // One semaphore count = one task, so we HAVE to release it.
            mTaskSemaphore.release();
        }
        --mRunningThreads;
    }
    thisThread->mActive = false;
}

void vorb::core::ThreadPool::setSize(ui32 size) {
    const i32 diff = size - mActiveThreads;
    if (diff < 0) {
        for (ui32 i = 0; i < (ui32)(-diff); ++i) {
            assert(mWorkers.size());
            --mActiveThreads;
            // We do not queue a task, we simply let the worker die next time it wakes up
            mWorkers.back()->mStop.store(true);
            mDeadWorkers.emplace_back(std::move(mWorkers.back()));
            mWorkers.pop_back();
        }
        // Note that we return, so we will not clear finished threads below.
        // That is fine!
        return;
    }
    else if (diff > 0) {
        for (ui32 i = 0; i < diff; ++i) {
            ++mActiveThreads;
            mWorkers.emplace_back(std::make_unique<WorkerThread>(&ThreadPool::workerThreadFunc, this));
        }
    }

    // Clear any finished threads
    for (size_t i = 0; i < mDeadWorkers.size();) {
        if (!mDeadWorkers[i]->mActive) {
            mDeadWorkers[i]->join();
            mDeadWorkers[i] = std::move(mDeadWorkers.back());
            mDeadWorkers.pop_back();
        }
        else {
            ++i;
        }
    }
}