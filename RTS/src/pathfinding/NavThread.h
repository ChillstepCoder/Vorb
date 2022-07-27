#pragma once

#include <Vorb/concurrentqueue.h>
#include <Vorb/blockingconcurrentqueue.h>

#include "PathFinder.h"

class World;
class TileContainer;
class Building;

enum class PathRequestType {
    TERRAIN_FINE,
    TERRAIN_COARSE,
    BUILDING_FINE,
    COUNT
};

struct TerrainPathArgs {
    PathPoint start;
    PathPoint goal;
};
struct BuildingPathArgs {
    const Building* building;
    TileIndex start;
    TileIndex goal;
};

struct PathArgs {
    PathArgs() {};
    PathArgs(std::shared_ptr<NavPath>& pathToBuild, const PathPoint& start, const PathPoint& end, bool isCoarse) :
        pathToBuild(pathToBuild),
        terrainArgs({ start, end }) {
        if (isCoarse) {
            type = PathRequestType::TERRAIN_COARSE;
        }
        else {
            type = PathRequestType::TERRAIN_FINE;
        }
    }
    PathArgs(std::shared_ptr<NavPath>& pathToBuild, const TileIndex start, const TileIndex end, const Building* building) :
        pathToBuild(pathToBuild),
        buildingArgs({ building, start, end }),
        type(PathRequestType::BUILDING_FINE) {
    };

    std::shared_ptr<NavPath> pathToBuild;
    union {
        TerrainPathArgs terrainArgs;
        BuildingPathArgs buildingArgs;
    };
    PathRequestType type;
};

// TODO: we can definitely replace std::function with a function pointer that takes TileContainerID as parameter
using NavThreadPathArgs = std::pair<PathArgs, std::function<void()>>;
using NavThreadGraphBuildArgs = std::pair<TileContainerID, std::function<void()>>;

class NavThread {
public:
    NavThread();
    ~NavThread();

    void init(World& world);

    void mainThreadUpdate();

    /// Clears all unprocessed tasks from the task queue
    void clearTasks();

    void addPathfindTask(std::shared_ptr<NavPath>& path, const PathPoint& start, const PathPoint& goal, bool isCoarse, std::function<void()>&& mainProc) {
        mPathTasks.enqueue(std::make_pair(PathArgs(path, start, goal, isCoarse), std::move(mainProc)));
    }
    void addPathfindTask(std::shared_ptr<NavPath>& path, const PathPoint& start, const PathPoint& goal, bool isCoarse) {
        mPathTasks.enqueue(std::make_pair(PathArgs(path, start, goal, isCoarse), nullptr));
    }
    void addPathfindTask(std::shared_ptr<NavPath>& path, const Building* building, TileIndex start, TileIndex goal, std::function<void()>&& mainProc) {
        mPathTasks.enqueue(std::make_pair(PathArgs(path, start, goal, building), std::move(mainProc)));
    }
    void addPathfindTask(std::shared_ptr<NavPath>& path, const Building* building, TileIndex start, TileIndex goal) {
        mPathTasks.enqueue(std::make_pair(PathArgs(path, start, goal, building), nullptr));
    }

    void addNavgraphBuildTask(TileContainer& tileContainer);

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

