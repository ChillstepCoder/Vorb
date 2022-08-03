#include "stdafx.h"
#include "NavThread.h"

#include "World.h"
#include "NavWorld.h"

#include "tile/TileContainer.h"

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
    mPathFinder = std::make_unique<PathFinder>(world);
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

void NavThread::addNavgraphBuildTask(TileContainer& tileContainer) {
    assert(IS_MAIN_THREAD());
    assert(!tileContainer.mIsNavmeshing.load());
    tileContainer.incReadLockAndRef();
    tileContainer.setDirtyNav(false);
    tileContainer.mIsNavmeshing.store(true);
    if (sDebugOptions.mShowNavGraphUpdates) {

        mNavGraphBuildTasks.enqueue(std::make_pair(tileContainer.getId(), [&]() {
            if (sDebugOptions.mShowNavGraphUpdates) {
                mWorld->getNavWorld().debugDrawNavGraphForContainer(tileContainer, 250);
            }
        }));
    }
    else {
        mNavGraphBuildTasks.enqueue(std::make_pair(tileContainer.getId(), nullptr));
    }
}

// Yield CPU resources
constexpr int64_t MAX_PATH_WAIT_TIME_MICROSECONDS = 3000;

void NavThread::navThreadFunc() {
    NAV_THREAD_ID = std::this_thread::get_id();

    NavThreadPathArgs pathArgs;
    NavThreadGraphBuildArgs graphArgs;
    NavWorld& navGraph = mWorld->getNavWorld();
    while (!mStop.load()) {
        // TODO: Super tiny chance of race condition here in isRunning(). We could dequeue a single task and be considered not running very briefly even tho we are
        mRunningPathfind = false;
        mRunningPathfind = mPathTasks.wait_dequeue_timed(pathArgs, MAX_PATH_WAIT_TIME_MICROSECONDS);
        bool hasTask = mRunningPathfind;

        // lazily generate ALL nav graphs
        //  TODO: We shouldnt  know about chunks or chunk IDs, just TileContainer
        while (mNavGraphBuildTasks.try_dequeue(graphArgs)) {
            TileContainer* tileContainer = TileContainerRepository::getTileContainer(graphArgs.first);
            assert(tileContainer);
            navGraph.buildNavGraphForContainer(*tileContainer);
            tileContainer->mIsNavmeshing.store(false);
            if (graphArgs.second) {
                mMainThreadProcs.enqueue(std::move(graphArgs.second));
            }
        }

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
                default:
                    assert(false);
            }
            static_assert(e_cast(PathRequestType::COUNT) == 2);
           
            if (pathArgs.second) {
                mMainThreadProcs.enqueue(std::move(pathArgs.second));
            }
        }
    }
    mRunningPathfind = false;
}
