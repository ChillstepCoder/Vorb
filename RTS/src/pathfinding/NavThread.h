#pragma once

#include <Vorb/concurrentqueue.h>
#include <Vorb/blockingconcurrentqueue.h>

#include "PathFinder.h"

class World;
class Chunk;

struct PathArgs {
    std::shared_ptr<NavPath> pathToBuild;
    PathPoint start;
    PathPoint goal;
    bool isCoarse;
};

using NavThreadPathArgs = std::pair<PathArgs, std::function<void()>>;
using NavThreadGraphBuildArgs = std::pair<ui32 /*chunkId*/, std::function<void()>>;

class NavThread {
public:
    NavThread();
    ~NavThread();

    void init(World& world);

    void mainThreadUpdate();

    /// Clears all unprocessed tasks from the task queue
    void clearTasks();

    void addPathfindTask(std::shared_ptr<NavPath>& path, const PathPoint& start, const PathPoint& goal, bool isCoarse, std::function<void()>&& mainProc) {
        mPathTasks.enqueue(std::make_pair(PathArgs{ path, start, goal, isCoarse }, std::move(mainProc)));
    }
    void addPathfindTask(std::shared_ptr<NavPath>& path, const PathPoint& start, const PathPoint& goal, bool isCoarse) {
        mPathTasks.enqueue(std::make_pair(PathArgs{ path, start, goal, isCoarse }, nullptr));
    }

    void addNavgraphBuildTask(Chunk& chunk);

    size_t getTasksSizeApprox() const { return mPathTasks.size_approx() + mNavGraphBuildTasks.size_approx(); }
    size_t getMainThreadQueuedProcsApprox() const { return mMainThreadProcs.size_approx(); }

    bool isRunningPathfind() const { return mPathTasks.size_approx() > 0 || mRunningPathfind; }

private:
    void navThreadFunc();

    PathFinder mPathFinder;
    World* mWorld = nullptr;
    std::atomic_bool mStop = false;
    std::atomic_bool mRunningPathfind = false;
    std::unique_ptr<std::thread> mThread;


    moodycamel::BlockingConcurrentQueue<NavThreadPathArgs> mPathTasks; ///< Holds tasks to execute
    moodycamel::ConcurrentQueue<NavThreadGraphBuildArgs> mNavGraphBuildTasks; ///< Holds tasks to execute
    moodycamel::ConcurrentQueue<std::function<void()>> mMainThreadProcs; ///< Contains functions to run on main thread after complete
};

