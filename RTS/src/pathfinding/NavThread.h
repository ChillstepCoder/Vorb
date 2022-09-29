#pragma once

#include <Vorb/concurrentqueue.h>
#include <Vorb/blockingconcurrentqueue.h>

#include "CoarseNavGraph.h"

#include "PathFinder.h"
#include "tile/TileHandle.h"

class TileContainer;
class Building;

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
    PathArgs(std::shared_ptr<NavPath>& pathToBuild, const TileHandle& start, const TileHandle& end, bool isCoarse) :
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
    TileHandle start;
    TileHandle goal;
    PathRequestType type;
};

// TODO: we can definitely replace std::function with a function pointer that takes TileContainerID as parameter
using NavThreadPathArgs = std::pair<PathArgs, std::function<void()>>;
struct NavThreadGraphBuildArgs {
    NavGraphTileDataToCopy navTileData;
    CoarseNavGraph navGraph;
    TileContainer* container;
};

class NavThread {
public:
    NavThread();
    ~NavThread();

    void init();

    void mainThreadUpdate();

    /// Clears all unprocessed tasks from the task queue
    void clearTasks();

    void addPathfindTask(std::shared_ptr<NavPath>& path, const TileHandle& start, const TileHandle& goal, bool isCoarse, std::function<void()>&& mainProc);
    void addPathfindTask(std::shared_ptr<NavPath>& path, const TileHandle& start, const TileHandle& goal, bool isCoarse);

    void addNavgraphBuildTask(TileContainer& tileContainer);

    size_t getTasksSizeApprox() const { return mPathTasks.size_approx() + mNavGraphBuildTasks.size_approx(); }
    size_t getMainThreadQueuedProcsApprox() const { return mMainThreadProcs.size_approx(); }

    bool isRunningPathfind() const { return mPathTasks.size_approx() > 0 || mRunningPathfind; }

private:
    void navThreadFunc();

    std::unique_ptr<PathFinder> mPathFinder;
    std::atomic_bool mStop = false;
    std::atomic_bool mRunningPathfind = false;
    std::unique_ptr<std::thread> mThread;


    moodycamel::BlockingConcurrentQueue<NavThreadPathArgs> mPathTasks; ///< Holds tasks to execute
    moodycamel::ConcurrentQueue<NavThreadGraphBuildArgs> mNavGraphBuildTasks; ///< Holds tasks to execute
    moodycamel::ConcurrentQueue<std::function<void()>> mMainThreadProcs; ///< Contains functions to run on main thread after complete
};

