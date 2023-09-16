#pragma once

class RenderContext;
class AssetLoader;

#include <any>

typedef std::function<bool(AssetLoader&, AssetID, const vio::Path&, void*, std::any&)> AssetLoadFunc;


// Return a new AssetHandleBundle if we are awaiting dependencies
#define ASSET_LOAD_LAMBDA(assetID, filePath, assetDataPtr) [&](AssetLoader& assetLoader, AssetID assetID, const vio::Path& filePath, void* assetDataPtr, std::any&) -> bool
#define ASSET_LOAD_LAMBDA(assetID, filePath, assetDataPtr, userData) [&](AssetLoader& assetLoader, AssetID assetID, const vio::Path& filePath, void* assetDataPtr, std::any& userData) -> bool
// TODO: Task pool?
struct AssetLoadTask {
    AssetID mAssetID = INVALID_ASSET_ID;
    void* mAssetDataPtr = nullptr;
    vio::Path mFilePath;
    AssetLoadFunc mLoadFunc = nullptr;
    AssetLoadFunc mRenderPostFunc = nullptr;
    std::atomic_bool* mIsFinishedFlagPtr = nullptr;
    std::any mUserData;
};
typedef std::unique_ptr<AssetLoadTask> AssetLoadTaskPtr;
