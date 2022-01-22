#pragma once

#include <Vorb/concurrentqueue.h>
#include <Vorb/blockingconcurrentqueue.h>

#include "PathFinder.h"

class World;

struct PathArgs {
    std::shared_ptr<NavPath> pathToBuild;
    ui32v2 start;
    ui32v2 goal;
    bool isCoarse;
};

using NavThreadPathArgs = std::pair<PathArgs, std::function<void()>>;

class NavThread {
public:
    NavThread();
    ~NavThread();

    void init(const World& world);

    /// Clears all unprocessed tasks from the task queue
    void clearTasks();

    void addPathfindTask(std::shared_ptr<NavPath>& path, const ui32v2& start, const ui32v2& goal, bool isCoarse, std::function<void()>&& mainProc) {
        mTasks.enqueue(std::make_pair(PathArgs{path, start, goal, isCoarse}, std::move(mainProc)));
    }
    void addPathfindTask(std::shared_ptr<NavPath>& path, const ui32v2& start, const ui32v2& goal, bool isCoarse) {
        mTasks.enqueue(std::make_pair(PathArgs{ path, start, goal, isCoarse }, nullptr));
    }

    size_t getTasksSizeApprox() const { return mTasks.size_approx(); }

private:
    void navThreadFunc();

    PathFinder mPathFinder;
    const World* mWorld = nullptr;
    std::atomic_bool mStop = false;
    std::unique_ptr<std::thread> mThread;


    moodycamel::BlockingConcurrentQueue<NavThreadPathArgs> mTasks; ///< Holds tasks to execute
    moodycamel::ConcurrentQueue<std::function<void()>> mMainThreadProcs; ///< Contains functions to run on main thread after complete
};

