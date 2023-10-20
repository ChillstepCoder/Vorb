#pragma once


struct AssetMetadata {

    inline UniqueId64 getUUID() const { return mDescriptor.getUUID(); }
    inline AssetID getId() const { return mDescriptor.id; }
    inline AssetType getAssetType() const { return mDescriptor.assetType; }
    inline bool isValid() const { return mDescriptor.isValid(); }

    vio::Path mFilePath;
    StrToken mName;
    AssetDescriptor mDescriptor;

    // Not thread safe
    bool mRequestedLoad = false;
    //bool mFinishedLoading = false; // Or refcount needed to ensure we dont destroy this while
    // it is being loaded once we implement deallocation of assets
};
