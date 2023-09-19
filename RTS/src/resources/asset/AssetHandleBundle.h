#pragma once

#include "resources/asset/AssetHandleBase.h"
#include <boost/container/flat_map.hpp>

#include "util/BitArray.h"

template <IsAssetType T>
class IAssetRepository;

// Not thread safe. Ensure that the bundle is not modified while being used by the AssetLoader
class AssetHandleBundle {
public:
    inline bool hasAssetHandle(AssetDescriptor desc) const {
        return mContainedAssetDescriptors.find(desc) != mContainedAssetDescriptors.end();
    }
    inline bool hasAssetHandle(AssetID id, AssetType assetType) const {
        return mContainedAssetDescriptors.find(AssetDescriptor{ .id = id, .assetType = assetType }) != mContainedAssetDescriptors.end();
    }
    AssetHandleBase* tryGetAssetHandle(AssetDescriptor desc) {
        auto&& it = mContainedAssetDescriptors.find(desc);
        if (it == mContainedAssetDescriptors.end()) return nullptr;
        return mHandles[it->second].get();
    }
    AssetHandleBase* tryGetAssetHandle(AssetID id, AssetType assetType) {
        return tryGetAssetHandle(AssetDescriptor{ .id = id, .assetType = assetType });
    }
    void addAssetHandle(std::shared_ptr<AssetHandleBase> handle) {
        if (mLockedByAssetLoader) panic("Tried to add an asset handle to bundle being loaded by the asset loader!");

        assert(!hasAssetHandle(handle->getDescriptor()));
        mContainedAssetDescriptors.emplace(std::make_pair(handle->getDescriptor(), mHandles.size()));
        mLoaded.resizeAndZero(mContainedAssetDescriptors.size());
        if (handle->isLoaded()) {
            ++mLoadedCount;
            mLoaded.setBit(mContainedAssetDescriptors.size() - 1);
            mHandles.emplace_back(std::move(handle));
        }
        else {
            mHandles.emplace_back(std::move(handle));
        }
    }

    void reserveCount(size_t count) {
        mHandles.reserve(count);
        mContainedAssetDescriptors.reserve(count);
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
        assert(mLoadedCount == mHandles.size());
        return true;
    }

    // Must include IAssetRepository.h or this will not link
    template<typename T>
    const T& getLoadedAsset(StrToken assetName);
protected:
    friend class AssetLoader;
    void setLockedByAssetLoader(bool locked) {
        if (mLockedByAssetLoader) panic("Tried to double add asset bundle to asset loader!");
        mLockedByAssetLoader = locked;
    }

    boost::container::flat_map<AssetDescriptor, int> mContainedAssetDescriptors;
    std::vector<AssetHandleBasePtr> mHandles;
    int mLoadedCount = 0;
    bool mLockedByAssetLoader = false;
    BitArray mLoaded;
};
