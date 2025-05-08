#include "stdafx.h"
#include "AssetLoader.h"

#include "resources/IAssetRepository.h"

#include "rendering/RenderThreadTasks.h"
#include <Vorb/io/IOManager.h>

#include "resources/asset/AssetHandleBundle.h"

AssetLoader::AssetLoader() {
    const size_t numWorkerThreads = glm::clamp(std::thread::hardware_concurrency() / 2u, 1u, 6u);
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
        mLoadQueue.enqueue(std::make_unique<AssetLoadTask>(AssetLoadTask{ .mLoadFunc = [](AssetLoader&, AssetID, const vio::Path&, void*, std::any&) { return false; }}));
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
            it->first->setLockedByAssetLoader(false);
            requestAssetLoad(std::move(it->second));
            it = mTasksWaitingDependencies.erase(it);
        }
        else {
            ++it;
        }
    }
}

void AssetLoader::requestAssetLoadWithDependencies(AssetLoadFunc loadFunc, AssetLoadFunc renderPostFunc, AssetID assetId, void* assetData, const vio::Path& filePath, std::atomic_bool* isFinishedFlagPtr, std::any userData, AssetHandleBundle* dependencies) {
    if (dependencies) {
        dependencies->setLockedByAssetLoader(true);
        std::lock_guard lock(mDependencyMapMutex);
        mTasksWaitingDependencies.emplace(dependencies, std::make_unique<AssetLoadTask>(AssetLoadTask{ .mAssetID = assetId, .mAssetDataPtr = assetData, .mFilePath = filePath, .mLoadFunc = loadFunc, .mRenderPostFunc = renderPostFunc, .mIsFinishedFlagPtr = isFinishedFlagPtr, .mUserData = std::move(userData) }));
    } else {
        requestAssetLoad(std::make_unique<AssetLoadTask>(AssetLoadTask{ .mAssetID = assetId, .mAssetDataPtr = assetData, .mFilePath = filePath, .mLoadFunc = loadFunc, .mRenderPostFunc = renderPostFunc, .mIsFinishedFlagPtr = isFinishedFlagPtr, .mUserData = std::move(userData) }));
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
        std::any mUserData;
    };
    PostData* postData = new PostData();
    postData->mAssetID = task->mAssetID;
    postData->mPath = task->mFilePath;
    postData->mAssetDataPtr = task->mAssetDataPtr;
    postData->mRenderPostFunc = std::move(task->mRenderPostFunc);
    postData->mIsFinishedFlagPtr = task->mIsFinishedFlagPtr;
    postData->mUserData = std::move(task->mUserData);
    RenderThreadTasks::getInstance().addGenericTask([postData]() {
        if (postData->mRenderPostFunc(AssetLoader::getInstance(), postData->mAssetID, postData->mPath, postData->mAssetDataPtr, postData->mUserData)) {
            if (postData->mIsFinishedFlagPtr) {
                LOG_TRACE("    Finished load on render thread {} {}", postData->mAssetID, postData->mPath.getCString());
                postData->mIsFinishedFlagPtr->store(true);
            }
            else {
                LOG_WARN("    Finished load on render thread {} {} but no flag!", postData->mAssetID, postData->mPath.getCString());
            }
        }
        delete postData;
    });
}

void AssetLoader::workerThreadFunc(AssetLoader* loader) {

    AssetLoadTaskPtr task;
    nString dataStr;
    while (!mStop.load()) {
        // Note that threads will be stuck waiting here until the process ends
        mLoadQueue.wait_dequeue(task);
        LOG_TRACE("   Asset Loader dequeued task: {} {}", task->mAssetID, task->mFilePath.getCString());
        // We may only have a render func
        if (task->mLoadFunc) {
            if (task->mLoadFunc(*loader, task->mAssetID, task->mFilePath, task->mAssetDataPtr, task->mUserData)) {
                if (task->mRenderPostFunc) {
                    processRenderFunc(task);
                }
                else {
                    if (task->mIsFinishedFlagPtr) {
                        LOG_TRACE("    Finished load {} {}", task->mAssetID, task->mFilePath.getCString());
                        *task->mIsFinishedFlagPtr = true;
                    }
                }
            }
        }
        else {
            if (task->mRenderPostFunc) {
                processRenderFunc(task);
            }
            else if (task->mIsFinishedFlagPtr) {
                // If we get here, we just passed a task with no methods,
                // which means it was probably just pending dependencies
                LOG_TRACE("    Finished empty load {} {}", task->mAssetID, task->mFilePath.getCString());
                *task->mIsFinishedFlagPtr = true;
            }
        }
        task.reset();
        dataStr.clear();
    }
}
