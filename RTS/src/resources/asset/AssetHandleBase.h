#pragma once

class AssetHandleBase {
public:
    friend class IAssetRepositoryBase;

    AssetHandleBase() = default;
    virtual ~AssetHandleBase() = default;

    AssetID getAssetID() const { return mAssetID; }
    StrToken getName() const { return mAssetName; }

    bool isValid() const { return mAssetName.isValid(); }
    virtual bool isLoaded() const = 0;

    AssetDescriptor getDescriptor() const { return AssetDescriptor{ .id = mAssetID, .assetType = mAssetType }; }

protected:
    StrToken mAssetName;
    AssetID mAssetID = INVALID_ASSET_ID;
    AssetType mAssetType = AssetType::COUNT;
};
// TODO: UniquePtr
using AssetHandleBasePtr = std::unique_ptr<AssetHandleBase>;
