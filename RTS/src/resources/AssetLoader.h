#pragma once

#include "resources/AssetLoadTask.h"

DECL_VIO(class IOManager);

class AssetLoader
{
public:
    AssetLoader(vio::IOManager& ioManager);
    ~AssetLoader();
    VORB_NON_COPYABLE(AssetLoader);

    static void initInstance(vio::IOManager& ioManager) {
        assert(!sInstance);
        sInstance = std::make_unique<AssetLoader>(ioManager);
    }
    static AssetLoader& getInstance() {
        return *sInstance;
    }

    void requestAssetLoad(AssetLoadFunc loadFunc, AssetID assetId, void* assetData, const vio::Path& filePath, std::atomic_bool* isFinishedFlagPtr) {
        // TODO: Singleton pool
        mLoadQueue.enqueue(std::make_unique<AssetLoadTask>(AssetLoadTask{.mAssetID=assetId, .mAssetDataPtr=assetData, .mFilePath = filePath, .mLoadFunc = loadFunc, .mIsFinishedFlagPtr=isFinishedFlagPtr }));
    }

protected:
    // Typedef for func ptr
    typedef void (AssetLoader::* workerFunc)();


    class WorkerThread {
    public:
        /// Creates the thread
        /// @param func: The function the thread should execute
        WorkerThread(workerFunc func, AssetLoader* threadPool) {
            thread = std::make_unique<std::thread>(func, threadPool);
        }

        ~WorkerThread() {

        }
        /// Blocks until the worker thread completes
        void join() {
            thread->join();
        }

        std::unique_ptr<std::thread> thread; ///< The thread handle
    };

    void workerThreadFunc();

    vio::IOManager& mIOManager;
    std::atomic_bool mStop = false;
    moodycamel::BlockingConcurrentQueue<AssetLoadTaskPtr> mLoadQueue;
    std::vector<std::unique_ptr<WorkerThread>> mWorkers; ///< All the worker threads

    inline static std::unique_ptr<AssetLoader> sInstance = nullptr;
};

