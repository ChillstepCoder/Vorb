#pragma once

#include "resources/AssetLoadTask.h"
#include <Vorb/blockingconcurrentqueue.h>

DECL_VIO(class IOManager);

class AssetHandleBundle;

class AssetLoader
{
public:
    AssetLoader();
    ~AssetLoader();
    VORB_NON_COPYABLE(AssetLoader);

    void update();

    void requestAssetLoadWithDependencies(AssetLoadFunc loadFunc, AssetLoadFunc renderPostFunc, AssetID assetId, void* assetData, const vio::Path& filePath, std::atomic_bool* isFinishedFlagPtr, std::any userData, AssetHandleBundle* dependencies);

    static void initInstance() {
        assert(!sInstance);
        sInstance = std::make_unique<AssetLoader>();
    }
    static AssetLoader& getInstance() {
        return *sInstance;
    }

    void requestAssetLoad(AssetLoadFunc loadFunc, AssetLoadFunc renderPostFunc, AssetID assetId, void* assetData, const vio::Path& filePath, std::atomic_bool* isFinishedFlagPtr, std::any userData) {
        // TODO: Singleton pool
        LOG_TRACE("Request load {} {}", assetId, filePath.getCString());
        mLoadQueue.enqueue(std::make_unique<AssetLoadTask>(AssetLoadTask{.mAssetID=assetId, .mAssetDataPtr=assetData, .mFilePath = filePath, .mLoadFunc=loadFunc, .mRenderPostFunc=renderPostFunc, .mIsFinishedFlagPtr=isFinishedFlagPtr, .mUserData=std::move(userData) }));
    }

    void requestAssetLoad(std::unique_ptr<AssetLoadTask>&& task) {
        // TODO: Singleton pool
        LOG_TRACE("Request load {} {}", task->mAssetID, task->mFilePath.getCString());
        mLoadQueue.enqueue(std::move(task));
    }

    size_t getQueuedProcsApprox() const { return mLoadQueue.size_approx(); }

protected:
    // Typedef for func ptr
    typedef void (AssetLoader::* workerFunc)(AssetLoader*);


    class WorkerThread {
    public:
        /// Creates the thread
        /// @param func: The function the thread should execute
        WorkerThread(workerFunc func, AssetLoader* loader) {
            thread = std::make_unique<std::thread>(func, loader, loader); // TODO: Why double loader?
        }

        ~WorkerThread() {

        }
        /// Blocks until the worker thread completes
        void join() {
            thread->join();
        }

        std::unique_ptr<std::thread> thread; ///< The thread handle
    };

    void workerThreadFunc(AssetLoader* loader);

    std::atomic_bool mStop = false;
    moodycamel::BlockingConcurrentQueue<AssetLoadTaskPtr> mLoadQueue;
    std::vector<std::unique_ptr<WorkerThread>> mWorkers; ///< All the worker threads

    std::mutex mDependencyMapMutex;
    UnorderedFlatMap<AssetHandleBundle*, AssetLoadTaskPtr> mTasksWaitingDependencies;

    inline static std::unique_ptr<AssetLoader> sInstance = nullptr;
};

