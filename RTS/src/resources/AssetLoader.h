#pragma once

typedef std::function<void(AssetID, const vio::Path&, std::string_view, void*)> AssetLoadFunc;

struct AssetLoadTask {
    AssetID mAssetID = INVALID_ASSET_ID;
    void* mAssetDataPtr = nullptr;
    vio::Path mFilePath;
    AssetLoadFunc mLoadFunc;
    std::atomic_bool* mIsFinishedFlagPtr = nullptr;
};
typedef std::unique_ptr<AssetLoadTask> AssetLoadTaskPtr;

DECL_VIO(class IOManager);

class AssetLoader
{
public:
    AssetLoader(vio::IOManager& ioManager);
    ~AssetLoader();
    VORB_NON_COPYABLE(AssetLoader);

    void initInstance(vio::IOManager& ioManager) {
        sInstance = std::make_unique<AssetLoader>(mIOManager);
        assert(!sInstance);
    }
    AssetLoader& getInstance() {
        return *sInstance;
    }

    void requestAssetLoad(AssetLoadFunc loadFunc, AssetID assetId, void* assetData, const vio::Path& filePath, std::atomic_bool* isFinishedFlagPtr) {
        // TODO: Singleton pool
        mLoadQueue.enqueue(std::make_unique<AssetLoadTask>(AssetLoadTask{.mAssetID=assetId, .mAssetDataPtr=assetData, .mFilePath = filePath, .mLoadFunc = loadFunc}));
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

