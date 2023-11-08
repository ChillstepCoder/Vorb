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
    std::function<void()> callback = nullptr;
    TileContainer* tileContainer = nullptr;
    VisibilityThreadTaskType taskType = VisibilityThreadTaskType::Init;
};

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
        mTasks.enqueue(VisibilityThreadTask{ nullptr, &container, VisibilityThreadTaskType::Init });
    }
    void visThreadFunc();

    moodycamel::BlockingConcurrentQueue<VisibilityThreadTask> mTasks;
    moodycamel::ConcurrentQueue<std::function<void()>> mMainThreadProcs;

    std::atomic_bool mStop = false;
    std::unique_ptr<std::thread> mThread;
    ThreadUtilizationTimer mThreadUtilizationTimer;
};