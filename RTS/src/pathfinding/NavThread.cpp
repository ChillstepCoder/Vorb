#include "stdafx.h"
#include "NavThread.h"

NavThread::NavThread() {
}

NavThread::~NavThread() {
    if (mThread) {
        mStop.store(true);
        mThread->join();
    }
}

void NavThread::init(const World& world) {
    assert(!mThread); // No double init
    mWorld = &world;
    if (!mThread) {
        mThread = std::make_unique<std::thread>(&NavThread::navThreadFunc, this);
    }
}

void NavThread::clearTasks() {
    // Dequeue all tasks
    NavThreadPathArgs args;
    while (mTasks.try_dequeue(args));
}

void NavThread::navThreadFunc() {

    NavThreadPathArgs args;
    while (!mStop.load()) {
        mTasks.wait_dequeue(args);
        if (args.first.isCoarse) {
            mPathFinder.generateCoarsePathSynchronous(*mWorld, args.first.start, args.first.goal, *args.first.pathToBuild);
        }
        else {
            mPathFinder.generatePathSynchronous(*mWorld, args.first.start, args.first.goal, *args.first.pathToBuild);
        }
        if (args.second) {
            mMainThreadProcs.enqueue(std::move(args.second));
        }
    }
}
