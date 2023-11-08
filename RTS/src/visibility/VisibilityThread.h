#pragma once

#include <Vorb/concurrentqueue.h>
#include <Vorb/blockingconcurrentqueue.h>
#include "util/Timing/ThreadUtilizationTimer.h"

class TileContainer;
class World;

enum class VisibilityThreadTaskType {
    Init,
};

struct VisibilityThreadTask {
    TileContainerID tileContainer = nullptr;
    VisibilityThreadTaskType taskType = VisibilityThreadTaskType::Init;
};
SIZER(VisibilityThreadTask);

class VisibilityThread {
    friend class VisibilityManager;
public:
    VisibilityThread();
    ~VisibilityThread();

    static void initInstance();
    static void shutdown();
    static bool hasInstance();
    static VisibilityThread& getInstance();

    void mainThreadUpdate();

    const ThreadUtilizationTimer& getThreadUtilizationTimer() const { return mThreadUtilizationTimer; }
private:
    void addInitContainerVisibilityTask(TileContainer& container) {
        mTasks.enqueue(VisibilityThreadTask{ &container, VisibilityThreadTaskType::Init });
    }
    void visThreadFunc();

    moodycamel::BlockingConcurrentQueue<VisibilityThreadTask> mTasks;
    moodycamel::ConcurrentQueue<std::function<void()>> mMainThreadProcs;

    std::atomic_bool mStop = false;
    std::unique_ptr<std::thread> mThread;
    ThreadUtilizationTimer mThreadUtilizationTimer;
};