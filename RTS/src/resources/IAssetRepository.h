#pragma once

#include "resources/IAsset.h"
#include "util/ExclusiveCacheLine.h"

class AssetLoader;

template <IsAssetType T>
class AssetHandle;

DECL_VIO(class IOManager);

class IAssetRepositoryBase {
public:
    virtual ~IAssetRepositoryBase() = default;
protected:
    IAssetRepositoryBase(vio::IOManager& ioManager) : mIoManager(ioManager) {}
    virtual ~IAssetRepositoryBase() = default;

    vio::IOManager& mIoManager;
    bool saveAssetContents(const IAsset& asset, const char* fileContents, size_t sizeBytes);
};

struct AssetRegistryEntry {
    StrToken mName;
    vio::Path mFilePath;
    std::atomic_int mRefCount = 0;
    bool mRequestedLoad = false;
    //bool mFinishedLoading = false; // Or refcount needed to ensure we dont destroy this while
    // it is being loaded once we implement deallocation of assets
};

template <IsAssetType T>
class IAssetRepository : public IAssetRepositoryBase {
public:
    virtual ~IAssetRepositoryBase() = default;

    virtual void initInstance(vio::IOManager& ioManager) = 0;
    IAssetRepository<T>& getInstance() {
        assert(sInstance);
        return *sInstance;
    }

    const std::vector<T>& getAllAssets() const { return mAssets; }
    std::vector<T>& getAllAssetsMutable() { return mAssets; }
    const std::map<StrToken, AssetID>& getAssetNames() const { return mAssetRegistry; }

    // Will begin a lazy async load if asset is not loaded
    AssetHandle<T> getAssetHandle(AssetID id);
    AssetHandle<T> getAssetHandle(StrToken assetName);
    AssetID getAssetID(StrToken assetName) { return mAssetRegistry[assetName].mAssetID; }

    AssetID registerAsset(StrToken name, const vio::Path& filePath) {
        AssetID id = mAssets.size();
        assert(mAssetRegistry.find(name) == mAssetRegistry.end());
        mAssetLookup[name] = id;
        mAssetRegistry[id] = AssetRegistryEntry{ .mFilePath(filePath) };
        mAssets.emplace_back(name, id);
        return id;
    }

    // Editor function which will register and create a default asset of this type
    T* editorTryAddNewAsset(StrToken name) {
        if (mAssetRegistry.find(name) != mAssetRegistry.end()) {
            return nullptr;
        }
        AssetID id;
        if (mFreeIDs.size()) {
            AssetID id = mFreeIDs.back();
            mFreeIDs.pop_back();
            mAssetLookup[name] = id;
            mAssetRegistry[id] = AssetRegistryEntry{ .mRequestedLoad(true) /*Already loaded*/};
            mAssets[id] = T(name, id);
            return &mAssets[id];
        }
        else {
            AssetID id = mAssets.size();
            mAssetLookup[name] = id;
            mAssetRegistry[id] = AssetRegistryEntry{ .mRequestedLoad(true) /*Already loaded*/ };
            T& newAsset = mAssets.emplace_back(name, id);
            return &newAsset;
        }
    }
    
    void deleteAsset(AssetID id) {
        // No double free
        for (AssetID freeId : mFreeIDs) {
            if (freeId == id) {
                return;
            }
        }
        mFreeIDs.emplace_back(id);
        mAssetRegistry.erase(mAssets[id].getName());
        // TODO: Remove file
    }
    void aquireAssetHandle(StrToken assetName, AssetHandle<T>& handle) {
        assert(!handle.isValid());
        aquireAssetHandle(mAssetRegistry[assetName].mAssetID, handle);
    }
    void pollAsset();
    void aquireAssetHandle(AssetID id, AssetHandle<T>& handle) {
        assert(!handle.isValid());
        handle.mAssetID = id;
        handle.mAssetName = mAssets[id].getName();
        mAssetRegistry[handle.mAssetID].mRefCount++;
        if (mAssetRegistry[handle.mAssetID].mRequestedLoad) {
            if (mLoadedAssets[handle.mAssetID].load()) {
                handle.mLoadedAsset = &mAssets[handle.mAssetID];
            }
        }
        else {
            mAssetRegistry[handle.mAssetID].mRequestedLoad = true;
            loadAsset(AssetLoader(*this), mAssetRegistry[handle.mAssetID].mFilePath);
        }
    }
    void releaseAssetHandle(AssetHandle<T>& handle) {
        if (handle.isValid()) {
            mAssetRegistry[handle.mAssetID].mRefCount--;
            handle.mAssetName = {};
            handle.mAssetID = INVALID_ASSET_ID;
            handle.mLoadedAsset = nullptr;
        }
    }

protected:
    IAssetRepository(vio::IOManager& ioManager) : IAssetRepositoryBase(ioManager) {}
    virtual ~IAssetRepository() = default;

    virtual void loadAsset(AssetLoader& assetLoader, const vio::Path& filePath) = 0;
    virtual bool saveAsset(AssetID assetId) = 0;

    //virtual void loadAssetInternalFromData(const nString& data) = 0;

    vio::Path getAssetFilePath(AssetID id) const { return mAssetRegistry[id].mFilePath; }
    // TODO: Handle deleting old file?
    void changeAssetFilePath(const vio::Path& newPath) { mAssetRegistry[id].mFilePath = newPath; }

    std::map<StrToken, AssetID> mAssetLookup;
    std::vector<AssetRegistryEntry> mAssetRegistry;
    std::vector<T> mAssets;
    std::vector<ExclusiveCacheLine<std::atomic_bool>> mLoadedAssets;
    std::vector<AssetID> mFreeIDs;

    inline static std::unique_ptr<IAssetRepository<T>> sInstance;
    // mDirtyAssets?
};

template <IsAssetType T>
class AssetHandle {
public:
    friend class IAssetRepository<T>;
    ~AssetHandle() {
        IAssetRepository<T>::getInstance().releaseAssetHandle(*this);
    }

    VORB_NON_COPYABLE(AssetHandle);

    bool isValid() const { return mAssetName.isValid(); }
    bool isLoaded() const { return mLoadedAsset != nullptr; }
    // Returns nullptr if the asset is loading
    const T* tryGetAsset() const {
        assert(isValid());
        if (mLoadedAsset) return mLoadedAsset;
        // Query if its done loading
        return mLoadedAsset;
    }
    AssetID getAssetID() const { return mAssetID; }

    T* editorTryGetMutableAsset() const { return const_cast<T*>(mLoadedAsset); }

protected:
    // Called by IAssetRepository<T>
    AssetHandle(StrToken assetName, AssetID id, const T* asset) : mAssetName(assetName), mAssetID(id), mLoadedAsset(asset) {
        if (mLoadedAsset) {
            mLoadedAsset->incRef();
        }
    };

    StrToken mAssetName;
    const T* mLoadedAsset = nullptr;
    AssetID mAssetID = INVALID_ASSET_ID;
};


template <IsAssetType T>
AssetHandle<T> IAssetRepository<T>::getAssetHandle(StrToken assetName)
{
    AssetRegistryEntry& entry = mAssetRegistry.at(assetName)
}

template <IsAssetType T>
AssetHandle<T> IAssetRepository<T>::getAssetHandle(AssetID id)
{
    g;
}