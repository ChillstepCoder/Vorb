#pragma once

#include "resources/IAsset.h"

class AssetHandleBase {
public:
    friend class IAssetRepositoryBase;

    AssetHandleBase() = default;
    virtual ~AssetHandleBase() = default;

    AssetID getAssetID() const { return mAssetID; }

    bool isValid() const { return mAssetName.isValid(); }
    virtual bool isLoaded() = 0;

    AssetDescriptor getDescriptor() const { return AssetDescriptor{ .id = mAssetID, .assetType = mAssetType }; }

protected:
    StrToken mAssetName;
    AssetID mAssetID = INVALID_ASSET_ID;
    AssetType mAssetType = AssetType::COUNT;
};
// TODO: UniquePtr
using AssetHandleBasePtr = std::shared_ptr<AssetHandleBase>;
