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

    chunk.incRef();
    chunk.incRefNeighbors4();
    chunk.mIsNavmeshing.store(true);
    if (sDebugOptions.mShowNavGraphUpdates) {

        mNavGraphBuildTasks.enqueue(std::make_pair(chunk.getChunkID().id, [&]() {
            if (sDebugOptions.mShowNavGraphUpdates) {
                mWorld->getNavGraph().debugDrawNavGraphForChunk(chunk, 250);
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

    NavThreadPathArgs pathArgs;
    NavThreadGraphBuildArgs graphArgs;
    NavGraph& navGraph = mWorld->getNavGraph();
    while (!mStop.load()) {
        bool hasTask = mPathTasks.wait_dequeue_timed(pathArgs, MAX_PATH_WAIT_TIME_MICROSECONDS);

        // lazily generate ALL nav graphs
        while (mNavGraphBuildTasks.try_dequeue(graphArgs)) {
            Chunk& chunk = mWorld->getChunk(graphArgs.first);
            navGraph.buildNavNodesForChunkSynchronous(chunk);
            chunk.mIsNavmeshing.store(false);
            chunk.decRefNeighbors4();
            chunk.decRef();
            if (graphArgs.second) {
                mMainThreadProcs.enqueue(std::move(graphArgs.second));
            }
        }

        if (hasTask) {
            if (pathArgs.first.isCoarse) {
                mPathFinder.generateCoarsePathSynchronous(*mWorld, pathArgs.first.start, pathArgs.first.goal, *pathArgs.first.pathToBuild);
            }
            else {
                mPathFinder.generatePathSynchronous(*mWorld, pathArgs.first.start, pathArgs.first.goal, *pathArgs.first.pathToBuild);
            }
            if (pathArgs.second) {
                mMainThreadProcs.enqueue(std::move(pathArgs.second));
            }
        }
    }
}
