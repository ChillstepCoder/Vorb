#pragma once

#include "resources/IAsset.h"
#include "util/ExclusiveCacheLine.h"

#include <shared_mutex>

class AssetLoader;

template <IsAssetType T>
class AssetHandle;

DECL_VIO(class IOManager);

class IAssetRepositoryBase {
public:
    virtual ~IAssetRepositoryBase() = default;
protected:
    IAssetRepositoryBase(vio::IOManager& ioManager) : mIoManager(ioManager) {}

    vio::IOManager& mIoManager;
    bool saveAssetContents(const IAsset& asset, const vio::Path& path, const char* fileContents, size_t sizeBytes);
};

struct AssetRegistryEntry {
    StrToken mName;
    vio::Path mFilePath;
    AssetID mID = INVALID_ASSET_ID; // TODO: unneeded
    bool mRequestedLoad = false;
    //bool mFinishedLoading = false; // Or refcount needed to ensure we dont destroy this while
    // it is being loaded once we implement deallocation of assets
};

template <IsAssetType T>
class IAssetRepository : public IAssetRepositoryBase {
public:
    friend class AssetHandle<T>;

    virtual ~IAssetRepository() = default;

    inline static IAssetRepository<T>& getInstance() {
        assert(sInstance);
        return *sInstance;
    }

    // Not thread safe if if editor adds an asset during usage. Shouldnt happen
    const std::vector<std::unique_ptr<T>>& getAllAssets() const { return mAssets; }
    // Not thread safe if if editor adds an asset during usage. Shouldnt happen
    std::vector<std::unique_ptr<T>>& getAllAssetsMutable() { return mAssets; }
    const std::map<StrToken, AssetID>& getAssetNames() const { return mAssetLookup; }

    vio::Path getAssetFilePath(AssetID id) const { return mAssetRegistry[id].mFilePath; }
    // TODO: Handle deleting old file?
    void changeAssetFilePath(AssetID id, const vio::Path& newPath) { mAssetRegistry[id].mFilePath = newPath; }

    // Will begin a lazy async load if asset is not loaded
    AssetHandle<T> getAssetHandle(AssetID id);
    AssetHandle<T> getAssetHandle(StrToken assetName);
    AssetID getAssetID(StrToken assetName) { return mAssetRegistry[assetName].mAssetID; }
    AssetID registerAsset(const vio::Path& filePath) {
        // TODO remove string copy
        return registerAsset(StrToken(filePath.getFileNameNoExtension()), filePath);
    }

    // ALL assets must be registered before any are loaded, else we will have race conditions
    AssetID registerAsset(StrToken name, const vio::Path& filePath) {
        AssetID id = mAssets.size();
        assert(mAssetLookup.find(name) == mAssetLookup.end());
        mAssetLookup[name] = id;
        mAssetRegistry.emplace_back(AssetRegistryEntry{ .mName=name, .mFilePath=filePath, .mID=id});
        mAssetRefCounts.emplace_back();
        mAssets.emplace_back(std::make_unique<T>(name, id));
        return id;
    }

    // Editor function which will register and create a default asset of this type
    T* editorTryAddNewAsset(StrToken name) {
        if (mAssetLookup.find(name) != mAssetLookup.end()) {
            return nullptr;
        }
        AssetID id;
        if (mFreeIDs.size()) {
            AssetID id = mFreeIDs.back();
            mFreeIDs.pop_back();
            mAssetLookup[name] = id;
            mAssetRegistry[id] = AssetRegistryEntry{ .mID=id, .mRequestedLoad=true /*Already loaded*/};
            mAssetRefCounts[id] = 0;
            // Retain pointer stability by replacing previous asset directly
            *mAssets[id] = T(name, id);
            return mAssets[id].get();
        }
        else {
            AssetID id = mAssets.size();
            mAssetLookup[name] = id;
            mAssetRegistry.emplace_back(AssetRegistryEntry{ .mID=id, .mRequestedLoad=true /*Already loaded*/});
            mAssetRefCounts.emplace_back();
            T& newAsset = *mAssets.emplace_back(std::make_unique<T>(name, id));
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
        if (mAssetRefCounts[id] != 0) {
            LOG_WARN("Deleted an asset that still had refs: {}", mAssetRegistry[id].mName.toString());
        }
        mFreeIDs.emplace_back(id);
        mAssetLookup.erase(mAssets[id]->getName());
        // TODO: Remove file
    }

    virtual bool saveAsset(AssetID assetId) = 0;

protected:
    IAssetRepository(vio::IOManager& ioManager) : IAssetRepositoryBase(ioManager) {}

    virtual void loadAsset(AssetLoader& assetLoader, const vio::Path& filePath) = 0;

    // ==================================================================
    // Asset friend functions
    // ==================================================================
    void aquireAssetHandle(AssetID id, AssetHandle<T>& handle) {
        assert(!handle.isValid());
        handle.mAssetID = id;
        mAssetRegistry[handle.mAssetID].mRefCount++;
        if (mLoadedAssets[handle.mAssetID].load()) {
            handle.mAssetName = mAssets[id].getName();
            handle.mLoadedAsset = mAssets[id].get();
            return;
        }
        { // This should be fairly rare
            std::lock_guard lock(mBeginLoadAssetMutex);
            if (!mAssetRegistry[handle.mAssetID].mRequestedLoad) {
                mAssetRegistry[handle.mAssetID].mRequestedLoad = true;
                lock.unlock();
                loadAsset(AssetLoader(*this), mAssetRegistry[handle.mAssetID].mFilePath);
            }
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
    void pollAsset(AssetHandle<T>& handle) {
        if (mLoadedAssets[handle.mAssetID]) {
            handle.mLoadedAsset = mAssets[handle.mAssetID];
        }
    }

    // ==================================================================
    // Data
    // ==================================================================
    std::map<StrToken, AssetID> mAssetLookup;
    std::vector<AssetRegistryEntry> mAssetRegistry;
    std::vector<std::atomic_int> mAssetRefCounts;
    std::vector<std::unique_ptr<T>> mAssets;
    std::vector<ExclusiveCacheLine<std::atomic_bool>> mLoadedAssets;
    std::vector<AssetID> mFreeIDs;

    std::mutex mBeginLoadAssetMutex;

    inline static std::unique_ptr<IAssetRepository<T>> sInstance;
    // mDirtyAssets?
};

template <IsAssetType T>
class AssetHandle {
public:
    friend class IAssetRepository<T>;

    AssetHandle() = default;
    ~AssetHandle() {
        release();
    }

    VORB_NON_COPYABLE(AssetHandle);


    void aquire(AssetID id) {
        IAssetRepository<T>::getInstance().aquireAssetHandle(id, *this);
    }
    void aquire(StrToken name) {
        IAssetRepository<T>::getInstance().aquireAssetHandle(name, *this);
    }
    void release() {
        IAssetRepository<T>::getInstance().releaseAssetHandle(*this);
    }

    bool isValid() const { return mAssetName.isValid(); }
    bool isLoaded() const { return mLoadedAsset != nullptr; }
    // Returns nullptr if the asset is loading
    const T* tryGetAsset() const {
        assert(isValid());
        if (mLoadedAsset) return mLoadedAsset;
        IAssetRepository<T>::getInstance().pollAsset(*this);
        return mLoadedAsset;
    }
    AssetID getAssetID() const { return mAssetID; }

    T* editorTryGetMutableAsset() const { return const_cast<T*>(tryGetAsset()); }

protected:

    StrToken mAssetName;
    const T* mLoadedAsset = nullptr;
    AssetID mAssetID = INVALID_ASSET_ID;
};


template <IsAssetType T>
AssetHandle<T> IAssetRepository<T>::getAssetHandle(StrToken assetName)
{
    AssetRegistryEntry& entry = mAssetRegistry.at(assetName);
}

template <IsAssetType T>
AssetHandle<T> IAssetRepository<T>::getAssetHandle(AssetID id)
{
    AssetHandle<T> handle;
    aquireAssetHandle(id, handle);
    return handle;
}

