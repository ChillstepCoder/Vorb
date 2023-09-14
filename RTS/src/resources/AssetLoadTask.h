#pragma once

class RenderContext;

typedef std::function<void(AssetID, const vio::Path&, void*)> AssetLoadFunc;
typedef std::function<void(RenderContext& renderContext, AssetID, void*)> AssetLoadRenderProcessFunc;

#define ASSET_LOAD_LAMBDA(assetID, filePath, assetDataPtr) [&](AssetID assetID, const vio::Path& filePath, void* assetDataPtr)
#define ASSET_LOAD_RENDER_PROCESS_LAMBDA(assetID, assetDataPtr) [&](RenderContext& renderContext, AssetID assetID, void* assetDataPtr)

// TODO: Task pool?
struct AssetLoadTask {
    AssetID mAssetID = INVALID_ASSET_ID;
    void* mAssetDataPtr = nullptr;
    vio::Path mFilePath;
    AssetLoadFunc mLoadFunc;
    AssetLoadRenderProcessFunc mRenderPostFunc = nullptr;
    std::atomic_bool* mIsFinishedFlagPtr = nullptr;
};
typedef std::unique_ptr<AssetLoadTask> AssetLoadTaskPtr;
