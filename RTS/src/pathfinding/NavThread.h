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
    COARSE_HARVESTABLE,
    COUNT
};

struct BuildingPathArgs {
    const Building* building;
    TileIndex start;
    TileIndex goal;
};

struct PathArgs {
    PathArgs() {};
    PathArgs(std::shared_ptr<NavPath>& pathToBuild, const f32v3& start, const f32v3& end, bool isCoarse) :
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
    PathArgs(std::shared_ptr<NavPath>& pathToBuild, const f32v3& start, TileHarvestable harvestable, f32 maxDistance) :
        pathToBuild(pathToBuild),
        start(start),
        goalHarvestable(harvestable),
        harvestableMaxDistance(maxDistance) {
        type = PathRequestType::COARSE_HARVESTABLE;
    }

    std::shared_ptr<NavPath> pathToBuild;
    f32v3 start;
    union {
        f32v3 goal; // COARSE or FINE
        struct {
            TileHarvestable goalHarvestable; // COARSE_HARVESTABLE
            f32 harvestableMaxDistance;
        };
    };
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

    void addPathfindTask(std::shared_ptr<NavPath>& path, const f32v3& start, const f32v3& goal, bool isCoarse, std::function<void()>&& mainProc);
    void addPathfindToHarvestableTask(std::shared_ptr<NavPath>& path, const f32v3& start, TileHarvestable harvestable, f32 maxDistance, std::function<void()>&& mainProc);

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

