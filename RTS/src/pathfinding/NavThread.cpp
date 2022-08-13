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
    TileContainerID id;
    assert(!tileContainer.mIsNavmeshing.load());
    id = tileContainer.getId();
    tileContainer.incReadLockAndRef();
    tileContainer.setDirtyNav(false);
    tileContainer.mIsNavmeshing.store(true);

    TileContainer* tileContainerPtr = &tileContainer;

    Services::Threadpool::ref().addTask([this, tileContainerPtr](ThreadPoolWorkerData* workderData) {
        NavThreadGraphBuildArgs buildArgs;
        CoarseNavGraph navGraph;
        NavGraphTileDataToCopy navTileData;

        buildArgs.container = tileContainerPtr;
        assert(tileContainerPtr);
        mWorld->getNavWorld().buildNavGraphForContainer(*tileContainerPtr, buildArgs.navGraph, buildArgs.navTileData);
        tileContainerPtr->mIsNavmeshing.store(false);
        mNavGraphBuildTasks.enqueue(std::move(buildArgs));
    }, nullptr);
}

// Yield CPU resources
constexpr int64_t MAX_PATH_WAIT_TIME_MICROSECONDS = 3000;

void NavThread::navThreadFunc() {
    NAV_THREAD_ID = std::this_thread::get_id();

    NavThreadPathArgs pathArgs;
    NavThreadGraphBuildArgs graphArgs;
    NavWorld& navWorld = mWorld->getNavWorld();
    while (!mStop.load()) {
        // TODO: Super tiny chance of race condition here in isRunning(). We could dequeue a single task and be considered not running very briefly even tho we are
        mRunningPathfind = false;
        mRunningPathfind = mPathTasks.wait_dequeue_timed(pathArgs, MAX_PATH_WAIT_TIME_MICROSECONDS);
        bool hasTask = mRunningPathfind;

        // Any finished navgraphs must be processed first
        while (mNavGraphBuildTasks.try_dequeue(graphArgs)) {
            // Assign graph
            navWorld.assignCoarseNavGraph(graphArgs.container->getId(), std::move(graphArgs.navGraph));
            // Notify tile data
            NavGraphTileDataToCopy& navTileData = graphArgs.navTileData;
            const std::vector<Tile>& tiles = graphArgs.container->getTiles();
            for (ui32 i = 0; i < navTileData.tileDjNodeIDs.size(); ++i) {
                const ui16 nodeId = navTileData.tileDjNodeIDs[i];
                if (nodeId == INVALID_DJ_NODE_ID) {
                    tiles[i].setNavNodeIndex(INVALID_NAV_NODE_INDEX);
                }
                else {
                    tiles[i].setNavNodeIndex(navTileData.djNodes[nodeId]);
                }
            }
            // Release resources
            graphArgs.container->decReadLockAndRef();
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
