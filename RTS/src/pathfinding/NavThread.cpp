#include "stdafx.h"
#include "NavThread.h"

#include "World.h"
#include "NavGraph.h"

#include "world/Chunk.h"

#include "options/DebugOptions.h"

NavThread::NavThread() {
}

NavThread::~NavThread() {
    if (mThread) {
        mStop.store(true);
        mThread->join();
    }
}

void NavThread::init(World& world) {
    assert(!mThread); // No double init
    mWorld = &world;
    if (!mThread) {
        mThread = std::make_unique<std::thread>(&NavThread::navThreadFunc, this);
    }
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

void NavThread::addNavgraphBuildTask(Chunk& chunk) {
    assert(!chunk.mIsNavmeshing.load());

    chunk.incReadLockAndRefCountNeighbors4AndSelf();
    chunk.mIsNavmeshing.store(true);
    if (sDebugOptions.mShowNavGraphUpdates) {

        mNavGraphBuildTasks.enqueue(std::make_pair(chunk.getChunkID().id, [&]() {
            if (sDebugOptions.mShowNavGraphUpdates) {
                mWorld->getNavGraph().debugDrawNavPatchForContainer(chunk, 250);
            }
        }));
    }
    else {
        mNavGraphBuildTasks.enqueue(std::make_pair(chunk.getChunkID().id, nullptr));
    }
}

// Yield CPU resources
constexpr int64_t MAX_PATH_WAIT_TIME_MICROSECONDS = 3000;

void NavThread::navThreadFunc() {
    NAV_THREAD_ID = std::this_thread::get_id();

    NavThreadPathArgs pathArgs;
    NavThreadGraphBuildArgs graphArgs;
    NavGraph& navGraph = mWorld->getNavGraph();
    while (!mStop.load()) {
        // TODO: Super tiny chance of race condition here in isRunning(). We could dequeue a single task and be considered not running very briefly even tho we are
        mRunningPathfind = false;
        mRunningPathfind = mPathTasks.wait_dequeue_timed(pathArgs, MAX_PATH_WAIT_TIME_MICROSECONDS);
        bool hasTask = mRunningPathfind;

        // lazily generate ALL nav graphs
        while (mNavGraphBuildTasks.try_dequeue(graphArgs)) {
            Chunk& chunk = mWorld->getChunk(graphArgs.first);
            navGraph.buildNavPatchForContainer(chunk);
            chunk.mIsNavmeshing.store(false);
            chunk.decReadLockAndRefCountNeighbors4AndSelf();
            if (graphArgs.second) {
                mMainThreadProcs.enqueue(std::move(graphArgs.second));
            }
        }

        if (hasTask) {
            switch (pathArgs.first.type) {
                case PathRequestType::TERRAIN_FINE: {
                    const TerrainPathArgs& args = pathArgs.first.terrainArgs;
                    mPathFinder.generateFinePathSynchronous(*mWorld, args.start, args.goal, *pathArgs.first.pathToBuild);
                    break;
                }
                case PathRequestType::TERRAIN_COARSE: {
                    const TerrainPathArgs& args = pathArgs.first.terrainArgs;
                    mPathFinder.generateCoarsePathSynchronous(*mWorld, args.start, args.goal, *pathArgs.first.pathToBuild);
                    break;
                }
                case PathRequestType::BUILDING_FINE: {
                    const BuildingPathArgs& args = pathArgs.first.buildingArgs;
                    mPathFinder.generateBuildingPathSynchronous(*args.building, args.start, args.goal, *pathArgs.first.pathToBuild);
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
    mRunningPathfind = false;
}
