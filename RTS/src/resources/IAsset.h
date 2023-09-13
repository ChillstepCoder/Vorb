#pragma once

#define DEFAULT_ASSET_CONSTRUCTOR(Type) \
    Type(StrToken name, AssetID id) : IAsset(name, id) {};

#include "util/StrToken.h"

enum class AssetType : ui8 {
    ParticleSystem,
    COUNT
};


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
concept IsAssetType = std::is_base_of<IAsset, T>::value;

struct AssetDescriptor {
    AssetID id = INVALID_ASSET_ID;
    AssetType assetType = AssetType::COUNT;

    bool operator<(const AssetDescriptor& other) const {
        return id < other.id && (assetType < other.assetType || id == other.id);
    }
};