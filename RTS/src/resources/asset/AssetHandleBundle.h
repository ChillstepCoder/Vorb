#pragma once

#include "resources/asset/AssetHandleBase.h"

#include "util/BitArray.h"

// Not thread safe. Ensure that the bundle is not modified while being used by the AssetLoader
class AssetHandleBundle {
public:
    inline bool hasAssetHandle(AssetDescriptor desc) const {
        return mContainedAssetDescriptors.find(desc) != mContainedAssetDescriptors.end();
    }
    inline bool hasAssetHandle(AssetID id, AssetType assetType) const {
        return mContainedAssetDescriptors.find(AssetDescriptor{ .id = id, .assetType = assetType }) != mContainedAssetDescriptors.end();
    }
    void addAssetHandle(std::shared_ptr<AssetHandleBase> handle) {
        if (mLockedByAssetLoader) panic("Tried to add an asset handle to bundle being loaded by the asset loader!");

        assert(!hasAssetHandle(handle->getDescriptor()));
        mContainedAssetDescriptors.emplace(handle->getDescriptor());
        mLoaded.resize(mContainedAssetDescriptors.size());
        if (handle->isLoaded()) {
            ++mLoadedCount;
            mLoaded.setBit(mContainedAssetDescriptors.size() - 1);
            mHandles.emplace_back(std::move(handle));
        }
        else {
            mHandles.emplace_back(std::move(handle));
        }
    }

    bool areAllAssetsLoaded() {
        if (mLoadedCount == mHandles.size()) return true;
        for (size_t i = 0; i < mHandles.size(); ++i) {
            if (!mLoaded.getBit(i)) {
                if (mHandles[i]->isLoaded()) {
                    mLoaded.setBit(i);
                    ++mLoadedCount;
                }
                else {
                    // We don't need to check every one if one fails
                    return false;
                }
            }
        };
        return mLoadedCount == mHandles.size();
    }
protected:
    friend class AssetLoader;
    void setLockedByAssetLoader(bool locked) {
        if (mLockedByAssetLoader) panic("Tried to double add asset bundle to asset loader!");
        mLockedByAssetLoader = locked;
    }

    std::set<AssetDescriptor> mContainedAssetDescriptors;
    std::vector<AssetHandleBasePtr> mHandles;
    int mLoadedCount = 0;
    bool mLockedByAssetLoader = false;
    BitArray mLoaded;
};
