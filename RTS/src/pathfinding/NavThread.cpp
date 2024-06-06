#include "stdafx.h"
#include "NavThread.h"

#include "world/World.h"
#include "NavWorld.h"
#include "NavPath.h"
#include "PathFinder.h"

#include "tile/TileContainer.h"

#include "debugging/DebugRenderer.h"
#include "options/DebugOptions.h"

NavThread::NavThread() {
}

NavThread::~NavThread() {
    if (mThread) {
        mStop.store(true);
        mThread->join();
    }
}

void NavThread::init(NavWorld& navWorld) {
    assert(!mThread); // No double init
    mNavWorld = &navWorld;
    if (!mThread) {
        mThread = std::make_unique<std::thread>(&NavThread::navThreadFunc, this);
    }
    mPathFinder = std::make_unique<PathFinder>(navWorld);
}

void NavThread::mainThreadUpdate() {
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
        std::cout << timer.stop() << " ms *** NAV SPIKE WARNING ***\n";
    }
}

void NavThread::clearTasks() {
    // Dequeue all tasks
    NavThreadPathArgs args;
    while (mPathTasks.try_dequeue(args));
}

void NavThread::addPathfindTask(std::shared_ptr<NavPath>& path, const f32v3& start, const f32v3& goal, bool isCoarse, std::function<void()>&& mainProc) {
    mPathTasks.enqueue(std::make_pair(PathArgs(path, start, goal, isCoarse), std::move(mainProc)));
}

void NavThread::addPathfindToHarvestableTask(std::shared_ptr<NavPath>& path, const f32v3& start, TileHarvestable harvestable, f32 maxDistance, std::function<void()>&& mainProc) {
    mPathTasks.enqueue(std::make_pair(PathArgs(path, start, harvestable, maxDistance), std::move(mainProc)));
}


// Yield CPU resources
constexpr int64_t MAX_PATH_WAIT_TIME_MICROSECONDS = 2000; // 3000

void NavThread::navThreadFunc() {
    NAV_THREAD_ID = std::this_thread::get_id();
    setThreadName("Nav");


    NavThreadPathArgs pathArgs;
    LOG_CRITICAL("TODO: Fix srvWorld assert in NavThread::navThreadFunc");
    // TODO: This assert happened three times (FAILED DYNAMIC_CAST. sWorld is valid but srvWorld is null)
    while (!mStop.load()) {
        mThreadUtilizationTimer.beginFrame();

        mThreadUtilizationTimer.beginSleep();
        const bool hasTask = mPathTasks.wait_dequeue_timed(pathArgs, MAX_PATH_WAIT_TIME_MICROSECONDS);
        mThreadUtilizationTimer.endSleep();

        mNavWorld->updateNavThread();
        
        if (hasTask) {
            const PathArgs& args = pathArgs.first;
            switch (args.type) {
                case PathRequestType::FINE: {
                    mPathFinder->generateFinePathSynchronous(args.start, args.goal, *args.pathToBuild);
                    break;
                }
                case PathRequestType::COARSE: {
                    mPathFinder->generateCoarsePathSynchronous(args.start, args.goal, *args.pathToBuild);
                    break;
                }
                case PathRequestType::COARSE_HARVESTABLE: {
                    assert(false && "REENABLE");
                    //mPathFinder->tryGenerateCoarsePathToClosestFreeHarvestableSynchronous(args.start, args.goalHarvestable, args.harvestableMaxDistance, *args.pathToBuild);
                    break;
                }
                default:
                    assert(false);
            }
            static_assert(e_cast(PathRequestType::COUNT) == 3);
           
            if (pathArgs.second) {
                mMainThreadProcs.enqueue(std::move(pathArgs.second));
            }
        }

    }
}
