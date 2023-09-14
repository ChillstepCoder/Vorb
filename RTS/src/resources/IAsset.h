#pragma once

// TODO: Stdafx?
#include "serialization/YmlSerializer.h"

#define DEFAULT_ASSET_CONSTRUCTOR(Type) \
    Type(StrToken name, AssetID id) : IAsset(name, id) {};

#include "util/StrToken.h"

enum class AssetType : ui8 {
    ParticleSystem,
    Texture,
    Cubemap,
    Brush,
    COUNT
};
SERIALIZABLE_ENUM_SAME_NAME(AssetType,
    pair{ AssetType::ParticleSystem, "particle_system"sv },
    pair{ AssetType::Texture, "texture"sv },
    pair{ AssetType::Cubemap, "cubemap"sv },
    pair{ AssetType::Brush, "brush"sv }
)
static_assert(e_count(AssetType) == 4);

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
        if (id != other.id) return id < other.id;
        return assetType < other.assetType;
    }
};