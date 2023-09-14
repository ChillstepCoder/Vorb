#include "stdafx.h"
#include "AssetLoader.h"

#include "resources/IAssetRepository.h"

#include "rendering/RenderThreadTasks.h"
#include <Vorb/io/IOManager.h>

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
        mLoadQueue.enqueue(std::make_unique<AssetLoadTask>(AssetLoadTask{ .mLoadFunc = [](AssetID, const vio::Path&, void*) { return; }}));
    }

    // Join all threads
    for (size_t i = 0; i < mWorkers.size(); i++) {
        mWorkers[i]->join();
        mWorkers[i].reset();
    }
}

void AssetLoader::update() {
    std::lock_guard lock(mDependencyMapMutex);
    for (auto&& it = mTasksWaitingDependencies.begin(); it != mTasksWaitingDependencies.end();) {
        if (it->first->areAllAssetsLoaded()) {
            requestAssetLoad(std::move(it->second));
            it = mTasksWaitingDependencies.erase(it);
        }
        else {
            ++it;
        }
    }
}

void processRenderFunc(AssetLoadTaskPtr& task) {
    // TODO: Singleton pool?
    struct PostData {
        AssetID mAssetID = INVALID_ASSET_ID;
        vio::Path mPath;
        void* mAssetDataPtr = nullptr;
        AssetLoadFunc mRenderPostFunc = nullptr;
        std::atomic_bool* mIsFinishedFlagPtr = nullptr;
    };
    PostData* postData = new PostData();
    postData->mAssetID = task->mAssetID;
    postData->mPath = task->mFilePath;
    postData->mAssetDataPtr = task->mAssetDataPtr;
    postData->mRenderPostFunc = std::move(task->mRenderPostFunc);
    postData->mIsFinishedFlagPtr = task->mIsFinishedFlagPtr;
    RenderThreadTasks::getInstance().addGenericTask([](RenderContext& renderContext, void* vPathHandle) {
        PostData* postData = static_cast<PostData*>(vPathHandle);
        postData->mRenderPostFunc(AssetLoader::getInstance(), postData->mAssetID, postData->mPath, postData->mAssetDataPtr);
        if (postData->mIsFinishedFlagPtr) {
            *postData->mIsFinishedFlagPtr = true;
        }
        delete postData;
    }, postData);
}

void AssetLoader::workerThreadFunc(AssetLoader* loader) {
    assert(loader == &AssetLoader::getInstance());

    AssetLoadTaskPtr task;
    nString dataStr;
    while (!mStop.load()) {
        // Note that threads will be stuck waiting here until the process ends
        mLoadQueue.wait_dequeue(task);
        // We may only have a render func
        if (task->mLoadFunc) {
            if (task->mLoadFunc(*loader, task->mAssetID, task->mFilePath, task->mAssetDataPtr)) {
                if (task->mRenderPostFunc) {
                    processRenderFunc(task);
                }
                else {
                    if (task->mIsFinishedFlagPtr) {
                        *task->mIsFinishedFlagPtr = true;
                    }
                }
            }
        }
        else {
            assert(task->mRenderPostFunc);
            processRenderFunc(task);
        }
        task.reset();
        dataStr.clear();
    }
}
