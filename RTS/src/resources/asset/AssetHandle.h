#pragma once

#include "resources/asset/AssetHandleBase.h"

template <IsAssetType T>
class IAssetRepository;

// Must include IAssetRepository.h to get definitions
template <typename T>
class AssetHandle : public AssetHandleBase {
public:
    friend class IAssetRepository<T>;

    AssetHandle() = default;
    ~AssetHandle() {
        release();
    }

    std::unique_ptr<AssetHandle<T>> clone() const;

    VORB_NON_COPYABLE(AssetHandle);

    void aquire(AssetID id);
    void aquire(StrToken name);

    void release();

    // AssetHandleBase interface
    bool isLoaded() const override;

    // Returns nullptr if the asset is loading
    const T* tryGetLoadedAsset() const;
    // Asset MUST already be loaded or this will crash
    const T& getLoadedAsset() const;

    T* editorTryGetMutableAsset() { return const_cast<T*>(tryGetLoadedAsset()); }

protected:
    const T* mLoadedAsset = nullptr;
};

template <typename T>
using AssetHandlePtr = std::unique_ptr<AssetHandle<T>>;

template <typename T>
class AssetRawPtr {
public:
    AssetRawPtr() = default;
    AssetRawPtr(AssetID id) : assetId(id) {};

    AssetRawPtr<T>& operator= (AssetID id) {
        assetId = id;
        return *this;
    }

    const T& getLoadedOrUnloadedAsset() const;


    bool isValid() const {
        return assetId != INVALID_ASSET_ID;
    }

    AssetID assetId = INVALID_ASSET_ID;
};