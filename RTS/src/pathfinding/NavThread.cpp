#include "stdafx.h"
#include "NavThread.h"

#include "world/srv/SrvWorldInterface.h"
#include "world/IWorld.h"
#include "NavWorld.h"

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

void NavThread::init(const NavWorld& navWorld) {
    assert(!mThread); // No double init
    assert(sWorld);
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

void NavThread::addPathfindTask(std::shared_ptr<NavPath>& path, const TileHandle& start, const TileHandle& goal, bool isCoarse, std::function<void()>&& mainProc)
{
    //if (sDebugOptions.mShowPaths) {
    //    DebugRenderer::drawFilledQuad(f32v3(start.getWorldPos3D()), f32v2(1.0f), color4(0.0f, 1.0f, 0.0f, 0.7f), 1000);
    //    DebugRenderer::drawFilledQuad(f32v3(goal.getWorldPos3D()), f32v2(1.0f), color4(1.0f, 1.0f, 0.0f, 0.7f), 1000);
    //}
    mPathTasks.enqueue(std::make_pair(PathArgs(path, start, goal, isCoarse), std::move(mainProc)));
}

void NavThread::addPathfindTask(std::shared_ptr<NavPath>& path, const TileHandle& start, const TileHandle& goal, bool isCoarse)
{
    //if (sDebugOptions.mShowPaths) {
    //    DebugRenderer::drawFilledQuad(f32v3(start.getWorldPos3D()), f32v2(1.0f), color4(0.0f, 1.0f, 0.0f, 0.7f), 1000);
    //    DebugRenderer::drawFilledQuad(f32v3(goal.getWorldPos3D()), f32v2(1.0f), color4(1.0f, 1.0f, 0.0f, 0.7f), 1000);
    //}
    mPathTasks.enqueue(std::make_pair(PathArgs(path, start, goal, isCoarse), nullptr));
}

void NavThread::addNavgraphBuildTask(TileContainer& tileContainer) {
    assert(IS_GAME_THREAD());
    TileContainerID id;
    assert(!tileContainer.mIsNavmeshing.load());
    id = tileContainer.getId();
    tileContainer.incReadLockAndRef();
    tileContainer.setDirtyNav(false);
    tileContainer.mIsNavmeshing.store(true);

    TileContainer* tileContainerPtr = &tileContainer;

    Services::Threadpool::ref().addTask([this, tileContainerPtr](ThreadPoolWorkerData* workerData) {
        NavThreadGraphBuildArgs buildArgs;
        CoarseNavGraph navGraph;
        NavGraphTileDataToCopy navTileData;

        buildArgs.container = tileContainerPtr;
        assert(tileContainerPtr);
        dynamic_cast<SrvWorldInterface*>(sWorld)->getNavWorld().buildNavGraphForContainer(*tileContainerPtr, buildArgs.navGraph, buildArgs.navTileData);
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
    SrvWorldInterface* srvWorld = dynamic_cast<SrvWorldInterface*>(sWorld);
    assert(srvWorld);
    NavWorld& navWorld = srvWorld->getNavWorld();
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
