#include "stdafx.h"
#include "VisibilityThread.h"

#include "tile/TileContainer.h"

static VisibilityThread* sInstance = nullptr;

VisibilityThread::VisibilityThread() {
    mThread = std::make_unique<std::thread>(&VisibilityThread::visThreadFunc, this);
}

VisibilityThread::~VisibilityThread() {
    if (mThread) {
        mStop.store(true);
        mTasks.enqueue(VisibilityThreadTask()); // Stop task
        mThread->join();
    }
    sInstance = nullptr;
}

void VisibilityThread::initInstance() {
    sInstance = new VisibilityThread();
}

void VisibilityThread::shutdown() {
    delete sInstance;
    sInstance = nullptr;
}

bool VisibilityThread::hasInstance() {
    return sInstance != nullptr;
}

VisibilityThread& VisibilityThread::getInstance() {
    assert(sInstance);
    return *sInstance;
}

void VisibilityThread::mainThreadUpdate() {
    std::function<void()> proc;
    // TODO: bulk dequeue?
    constexpr unsigned MAX_MS = 3;
    PreciseTimer timer;
    // TODO: Use optik for profiling
    while (mMainThreadProcs.try_dequeue(proc)) {
        proc();
        if (timer.stop() > MAX_MS) {
            break;
        }
    }
    if (timer.stop() > 20.0f) {
        std::cout << timer.stop() << " ms *** VIS SPIKE WARNING ***\n";
    }
}


void VisibilityThread::visThreadFunc() {
    VISIBILITY_THREAD_ID = std::this_thread::get_id();
    setThreadName("Visibility");

    VisibilityThreadTask task;

    LOG_CRITICAL("TODO: Fix srvWorld assert in NavThread::navThreadFunc");
    // TODO: This assert happened three times (FAILED DYNAMIC_CAST. sWorld is valid but srvWorld is null)
    while (!mStop.load()) {
        mThreadUtilizationTimer.beginFrame();

        mThreadUtilizationTimer.beginSleep();
        mTasks.wait_dequeue(task);
        mThreadUtilizationTimer.endSleep();

        if (task.tileContainer) {

            switch (task.taskType) {
                case VisibilityThreadTaskType::Init: {
                    task.tileContainer->setDidInitVisibility();
                    break;
                }
                default:
                    panic("Unknown vistask type");
                    break;
            }

            if (task.callback) {
                mMainThreadProcs.enqueue(std::move(task.callback));
            }
        }
    }
}
