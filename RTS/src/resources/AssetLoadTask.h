#pragma once

class RenderContext;
class AssetLoader;

#include <any>

typedef std::function<bool(AssetLoader&, AssetID, const vio::Path&, void*, std::any&)> AssetLoadFunc;

#define ASSET_LOAD_LAMBDA(assetID, filePath, assetDataPtr, userData) (AssetLoader& assetLoader, AssetID assetID, const vio::Path& filePath, void* assetDataPtr, std::any& userData) -> bool

// TO be used inside ASSET_LOAD_LAMBDA
// Usage: LOAD_DEPENDENCIES_HELPER(def, &, /*Post Load Code*/);
#define LOAD_DEPENDENCIES_HELPER(defName, defaultCapture, ...) \
if (defName.getDependencies()) { \
    assetLoader.requestAssetLoadWithDependencies([defaultCapture] ASSET_LOAD_LAMBDA(assetId, filePath, assetDataPtr) { \
        __VA_ARGS__ \
        return true; \
    }, nullptr, \
    assetId, \
    assetDataPtr, \
    filePath, \
    mLoadedAssets[assetId].get(), \
    nullptr, \
    defName.getDependencies() \
    ); \
    return false; \
} else { \
    __VA_ARGS__ \
    return true; \
}

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
