#include "stdafx.h"
#include "AssetLoader.h"

#include <Vorb/io/IOManager.h>
#include "rendering/RenderThreadTasks.h"

AssetLoader::AssetLoader() {
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
        task->mLoadFunc(task->mAssetID, task->mFilePath, task->mAssetDataPtr);
        if (task->mRenderPostFunc) {
            // TODO: Singleton pool?
            struct PostData {
                AssetID mAssetID = INVALID_ASSET_ID;
                void* mAssetDataPtr = nullptr;
                AssetLoadRenderProcessFunc mRenderPostFunc = nullptr;
                std::atomic_bool* mIsFinishedFlagPtr = nullptr;
            };
            PostData* postData = new PostData();
            postData->mAssetID = task->mAssetID;
            postData->mAssetDataPtr = task->mAssetDataPtr;
            postData->mRenderPostFunc = std::move(task->mRenderPostFunc);
            postData->mIsFinishedFlagPtr = task->mIsFinishedFlagPtr;
            RenderThreadTasks::getInstance().addGenericTask([](RenderContext& renderContext, void* vPathHandle) {
                PostData* postData = static_cast<PostData*>(vPathHandle);
                postData->mRenderPostFunc(renderContext, postData->mAssetID, postData->mAssetDataPtr);
                if (postData->mIsFinishedFlagPtr) {
                    *postData->mIsFinishedFlagPtr = true;
                }
                delete postData;
            }, postData);
        }
        else {
            if (task->mIsFinishedFlagPtr) {
                *task->mIsFinishedFlagPtr = true;
            }
        }
        task.reset();
        dataStr.clear();
    }
}
