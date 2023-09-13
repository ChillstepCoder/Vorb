#pragma once

typedef std::function<void(AssetID, const vio::Path&, std::string_view, void*)> AssetLoadFunc;

#define ASSET_LOAD_LAMBDA(assetID, filePath, fileData, assetDataPtr) [&](AssetID assetID, const vio::Path& filePath, std::string_view fileData, void* assetDataPtr)

struct AssetLoadTask {
    AssetID mAssetID = INVALID_ASSET_ID;
    void* mAssetDataPtr = nullptr;
    vio::Path mFilePath;
    AssetLoadFunc mLoadFunc;
    std::atomic_bool* mIsFinishedFlagPtr = nullptr;
};
typedef std::unique_ptr<AssetLoadTask> AssetLoadTaskPtr;
