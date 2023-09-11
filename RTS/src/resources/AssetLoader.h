#pragma once

typedef std::function<void(AssetID, const vio::Path&, std::string_view data)> AssetLoadFunc;

struct AssetLoadTask {
    AssetID mAssetID = INVALID_ASSET_ID;
    void* mAssetDataPtr = nullptr;
    vio::Path mFilePath;
    AssetLoadFunc mLoadFunc;
    std::atomic_bool* mIsFinishedFlagPtr = nullptr;
};
typedef std::unique_ptr<AssetLoadTask> AssetLoadTaskPtr;

class AssetLoader
{
public:
    AssetLoader() = default;
    ~AssetLoader() = default;

    void requestAssetLoad(AssetLoadFunc loadFunc, AssetID assetId, const vio::Path& filePath, std::atomic_bool* isFinishedFlagPtr) {
        // TODO: Singleton pool
        mLoadQueue.enqueue(std::make_unique<AssetLoadTask>(AssetLoadTask{.mAssetID=assetId, .mFilePath=filePath, .mLoadFunc=loadFunc}));
    }

protected:
    void onLoadFinished(std::unique_ptr<AssetLoadTask>&& assetTaskPtr) {
        *assetTaskPtr->mIsFinishedFlagPtr = true;
    }

    moodycamel::ConcurrentQueue<AssetLoadTaskPtr> mLoadQueue;
};

