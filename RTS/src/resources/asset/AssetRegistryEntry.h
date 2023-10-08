#pragma once

struct AssetRegistryEntry {
    StrToken mName;
    vio::Path mFilePath;
    AssetID mID = INVALID_ASSET_ID; // TODO: unneeded
    bool mRequestedLoad = false;
    //bool mFinishedLoading = false; // Or refcount needed to ensure we dont destroy this while
    // it is being loaded once we implement deallocation of assets
};