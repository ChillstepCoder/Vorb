#pragma once

#define DEFAULT_ASSET_CONSTRUCTOR(Type) \
    Type(StrToken name, AssetID id) : IAsset(name, id) {};

#include "util/StrToken.h"

class IAsset {
public:
    IAsset(StrToken name, AssetID id) : mName(name), mID(id) {};
    virtual ~IAsset() = default;

    VORB_MOVABLE(IAsset);

    vio::Path getDiskLocation() const { return mDiskLocation; }
    void setDiskLocation(vio::Path val) const { mDiskLocation = val; }

    StrToken getName() const { return mName; }
    void setName(StrToken name) { mName = name; }

    AssetID getID() const { return mID; }

    bool isDirty() const { return mDirty; }
    void setDirty(bool val) const { mDirty = val; }

    void incRef() const { ASSERT_GAME_THREAD(); ++mRefCount; }
    void decRef() const { ASSERT_GAME_THREAD(); --mRefCount; }
    int getRefCount() const { ASSERT_GAME_THREAD(); return mRefCount; }

protected:
    StrToken mName;
    AssetID mID = INVALID_ASSET_ID;
    mutable vio::Path mDiskLocation;
    mutable int mRefCount = 0;
    mutable bool mDirty = false;
};

template <typename T>
concept IsAssetType = std::derived_from<T, IAsset>;

template <typename T>
class AssetHandle {
public:
    AssetHandle() = default;
    AssetHandle(StrToken assetName) : mAssetName(assetName) {};
    ~AssetHandle() {
       if (mResolvedAsset) {
           mResolvedAsset->decRef();
       }
    }

    VORB_NON_COPYABLE_BUT_MOVABLE(AssetHandle);

    bool isValid() const { return mAssetName.isValid(); }
    bool isResolved() const { return mResolvedAsset != nullptr; }
    T* getResolvedAsset() const { return mResolvedAsset; }
    AssetID getResolvedAssetID() const { return mResolvedAsset ? mResolvedAsset->getID() : INVALID_ASSET_ID; }

    T* resolveAssetSynchonous();
    void resolveAssetAsynchonous();

protected:
    StrToken mAssetName;
    T* mResolvedAsset = nullptr;
};