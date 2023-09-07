#pragma once

#define DEFAULT_ASSET_CONSTRUCTOR(Type) \
    Type(const nString& name, AssetID id) : IAsset(name, id) {};

#include "util/StrToken.h"

class IAsset {
public:
    IAsset(const nString& name, AssetID id) : mName(name), mID(id) {};
    IAsset(nString&& name, AssetID id) : mName(std::move(name)), mID(id) {};
    virtual ~IAsset() = default;

    VORB_MOVABLE(IAsset);

    vio::Path getDiskLocation() const { return mDiskLocation; }
    void setDiskLocation(vio::Path val) const { mDiskLocation = val; }

    const nString& getName() const { return mName; }
    void setName(const nString& name) { mName = name; }
    void setName(nString&& name) { mName = std::move(name); }

    AssetID getID() const { return mID; }

    bool isDirty() const { return mDirty; }
    void setDirty(bool val) const { mDirty = val; }

    void incRef() const { ASSERT_GAME_THREAD(); ++mRefCount; }
    void decRef() const { ASSERT_GAME_THREAD(); --mRefCount; }
    int getRefCount() const { ASSERT_GAME_THREAD(); return mRefCount; }

protected:
    nString mName;
    AssetID mID = INVALID_ASSET_ID;
    mutable vio::Path mDiskLocation;
    mutable int mRefCount = 0;
    mutable bool mDirty = false;
};

template <typename T>
concept IsAssetType = std::derived_from<T, IAsset>;