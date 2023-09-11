#pragma once

#define DEFAULT_ASSET_CONSTRUCTOR(Type) \
    Type(StrToken name, AssetID id) : IAsset(name, id) {};

#include "util/StrToken.h"

class IAsset {
public:
    IAsset(StrToken name, AssetID id) : mName(name), mID(id) {};
    virtual ~IAsset() = default;

    VORB_MOVABLE(IAsset);

    StrToken getName() const { return mName; }
    void setName(StrToken name) { mName = name; }

    AssetID getID() const { return mID; }

    bool isDirty() const { return mDirty; }
    void setDirty(bool val) const { mDirty = val; }

protected:
    StrToken mName;
    AssetID mID = INVALID_ASSET_ID;
    mutable bool mDirty = false;
};

template <typename T>
concept IsAssetType = std::derived_from<T, IAsset>;
