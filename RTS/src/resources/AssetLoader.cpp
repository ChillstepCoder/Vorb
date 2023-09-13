#include "stdafx.h"
#include "AssetLoader.h"

#include <Vorb/io/IOManager.h>

AssetLoader::AssetLoader(vio::IOManager& ioManager) : mIOManager(ioManager) {
    size_t numWorkerThreads = 2;
    /// Allocate all threads
    mWorkers.resize(numWorkerThreads);
    for (ui32 i = 0; i < numWorkerThreads; i++) {
        mWorkers[i] = std::make_unique<WorkerThread>(&AssetLoader::workerThreadFunc, this);
    }
}

AssetLoader::~AssetLoader() {
    mStop.store(true);

    // Clear out the queue
    // Dequeue all tasks
    AssetLoadTaskPtr task[64];
    while (mLoadQueue.try_dequeue_bulk(task, 64));

    // Tell all threads to wake up and close, as they are currently hanging on a semaphore
    for (size_t i = 0; i < mWorkers.size(); i++) {
        mLoadQueue.enqueue(std::make_unique<AssetLoadTask>(AssetLoadTask{ .mLoadFunc = [](AssetID, const vio::Path&, std::string_view, void*) { return; }}));
    }

    // Join all threads
    for (size_t i = 0; i < mWorkers.size(); i++) {
        mWorkers[i]->join();
        mWorkers[i].reset();
    }
}

void AssetLoader::workerThreadFunc() {
    AssetLoadTaskPtr task;
    nString dataStr;
    while (!mStop.load()) {
        // Note that threads will be stuck waiting here until the process ends
        mLoadQueue.wait_dequeue(task);
        if (!mIOManager.readFileToString(task->mFilePath, dataStr)) {
            panic("Asset loader thread failed to read file {}", task->mFilePath.getCString());
        }
        task->mLoadFunc(task->mAssetID, task->mFilePath, std::string_view(dataStr.data(), dataStr.length()), task->mAssetDataPtr);
        if (task->mIsFinishedFlagPtr) {
            *task->mIsFinishedFlagPtr = true;
        }
        task.reset();
        dataStr.clear();
    }
}
