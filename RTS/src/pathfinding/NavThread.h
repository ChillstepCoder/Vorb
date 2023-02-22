#pragma once

#include <Vorb/concurrentqueue.h>
#include <Vorb/blockingconcurrentqueue.h>

#include "CoarseNavGraph.h"

#include "PathFinder.h"
#include "tile/TileHandle.h"

class TileContainer;
class Building;
class NavWorld;

enum class PathRequestType {
    FINE,
    COARSE,
    COUNT
};

struct BuildingPathArgs {
    const Building* building;
    TileIndex start;
    TileIndex goal;
};

struct PathArgs {
    PathArgs() {};
    PathArgs(std::shared_ptr<NavPath>& pathToBuild, const LiteTileHandle& start, const LiteTileHandle& end, bool isCoarse) :
        pathToBuild(pathToBuild),
        start(start),
        goal(end) {
        if (isCoarse) {
            type = PathRequestType::COARSE;
        }
        else {
            type = PathRequestType::FINE;
        }
    }

    std::shared_ptr<NavPath> pathToBuild;
    LiteTileHandle start;
    LiteTileHandle goal;
    PathRequestType type;
};

// TODO: we can definitely replace std::function with a function pointer that takes TileContainerID as parameter
using NavThreadPathArgs = std::pair<PathArgs, std::function<void()>>;


class NavThread {
public:
    NavThread();
    ~NavThread();

    void init(NavWorld& navWorld);

    void mainThreadUpdate();

    /// Clears all unprocessed tasks from the task queue
    void clearTasks();

    void addPathfindTask(std::shared_ptr<NavPath>& path, const LiteTileHandle& start, const LiteTileHandle& goal, bool isCoarse, std::function<void()>&& mainProc);
    void addPathfindTask(std::shared_ptr<NavPath>& path, const LiteTileHandle& start, const LiteTileHandle& goal, bool isCoarse);

    size_t getTasksSizeApprox() const { return mPathTasks.size_approx(); }
    size_t getMainThreadQueuedProcsApprox() const { return mMainThreadProcs.size_approx(); }

private:
    void navThreadFunc();

    std::unique_ptr<PathFinder> mPathFinder;
    std::atomic_bool mStop = false;
    std::unique_ptr<std::thread> mThread;


    moodycamel::BlockingConcurrentQueue<NavThreadPathArgs> mPathTasks; ///< Holds tasks to execute
    moodycamel::ConcurrentQueue<std::function<void()>> mMainThreadProcs; ///< Contains functions to run on main thread after complete

    NavWorld* mNavWorld = nullptr;
};

