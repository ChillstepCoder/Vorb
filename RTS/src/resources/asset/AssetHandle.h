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
using AssetHandlePtr = std::shared_ptr<AssetHandle<T>>;