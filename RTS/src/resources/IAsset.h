#pragma once

#include "resources/asset/AssetType.h"

class IAssetRepositoryBase;

#define DEFAULT_ASSET_CONSTRUCTOR(Type, EType) \
    Type(StrToken name, AssetID id) : IAsset(name, id) {}; \
    AssetType getAssetType() const override { return EType; } \
    inline static constexpr AssetType ASSET_TYPE = EType;

class AssetHandleBundle;
class AssetHandleBase;

class IAsset {
public:
    IAsset(StrToken name, AssetID id);
    virtual ~IAsset();

    VORB_MOVABLE(IAsset);

    StrToken getName() const { return mName; }
    void setName(StrToken name) { mName = name; }

    AssetID getID() const { return mID; }
    virtual AssetType getAssetType() const = 0;

    bool isDirty() const { return mDirty; }
    void setDirty(bool val) const { mDirty = val; }
    AssetHandleBundle* getDependencies() const { return mDependencies.get(); }
    void addDependency(std::unique_ptr<AssetHandleBase>&& handle);
    void reserveDependencyCount(size_t count);

protected:
    std::unique_ptr<AssetHandleBundle> mDependencies;
    StrToken mName;
    AssetID mID = INVALID_ASSET_ID;
    mutable bool mDirty = false;
};

template <typename T>
concept IsAssetType = std::is_base_of<IAsset, T>::value;

// Represents a unique ID for an asset which can be used to look it up
struct AssetDescriptor {

    bool isValid() const { return assetType != AssetType::NONE; }
    
    bool operator==(const AssetDescriptor& other) const {
        return id == other.id && assetType == other.assetType;
    }
    bool operator<(const AssetDescriptor& other) const {
        if (id != other.id) return id < other.id;
        return assetType < other.assetType;
    }

    static AssetDescriptor fromUUID(UniqueId64 uid) {
        AssetDescriptor descriptor;
        descriptor.assetType = static_cast<AssetType>(uid >> 32ull);
        // Indicates probably not a valid UUID
        if (descriptor.assetType >= AssetType::NONE) {
            LOG_WARN("Tried to convert an invalid asset UID {} to an AssetDescriptor", static_cast<ui64>(uid));
            descriptor.assetType = AssetType::NONE;
            return descriptor;
        }
        descriptor.id = uid & 0xffffffffull;
        return descriptor;
    }

    UniqueId64 getUUID() const {
        return UniqueId64((ui64)id | ((ui64)assetType << 32ull));
    }
    StrToken getName() const;
    std::filesystem::path getPath() const;
    IAssetRepositoryBase* getRepo() const;

    // Data
    AssetID id = INVALID_ASSET_ID;
    AssetType assetType = AssetType::NONE;
};
