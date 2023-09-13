#pragma once

#include "resources/IAsset.h"
#include "util/ExclusiveCacheLine.h"

#include "resources/AssetLoadTask.h"
#include "resources/AssetLoader.h"

class AssetLoader;

template <IsAssetType T>
class AssetHandle;

template <IsAssetType T>
using AssetHandlePtr = std::shared_ptr<AssetHandle<T>>;

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

    IAssetRepository(vio::IOManager& ioManager) : IAssetRepositoryBase(ioManager) {}
    virtual ~IAssetRepository() = default;

    inline static IAssetRepository<T>& getInstance() {
        assert(sInstance);
        return *sInstance;
    }

    // ==================================================================
    // Public interface
    // ==================================================================

    void forEachLoadedAsset(std::function<bool(IAssetRepository<T>&, T&)> func) {
        for (AssetID id = 0; id < mAssets.size(); ++id) {
            if (mLoadedAssets[id]->load()) {
                if (func(*this, *mAssets[id])) return;
            }
        };
    }
    void forEachRegisteredAsset(std::function<bool(IAssetRepository<T>&, T*, const AssetRegistryEntry& entry)> func) {
        for (AssetID id = 0; id < mAssets.size(); ++id) {
            if (mLoadedAssets[id]->load()) {
                if (func(*this, mAssets[id].get(), mAssetRegistry[id])) return;
            }
            else {
                if (func(*this, nullptr, mAssetRegistry[id])) return;
            }
        };
    }

    vio::Path getAssetFilePath(AssetID id) const { return mAssetRegistry[id].mFilePath; }
    // TODO: Handle deleting old file?
    void changeAssetFilePath(AssetID id, const vio::Path& newPath) { mAssetRegistry[id].mFilePath = newPath; }

    // Will begin a lazy async load if asset is not loaded
    AssetHandlePtr<T> getAssetHandle(AssetID id);
    AssetHandlePtr<T> getAssetHandle(StrToken assetName);
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
        mAssetRefCounts.emplace_back(std::make_unique<ExclusiveCacheLine<std::atomic_int>>(0));
        mAssets.emplace_back(std::make_unique<T>(name, id));
        mLoadedAssets.emplace_back(std::make_unique<ExclusiveCacheLine<std::atomic_bool>>(false));
        return id;
    }

    // Editor function which will register and create a default asset of this type
    AssetHandlePtr<T> editorTryAddNewAsset(StrToken name) {
        if (mAssetLookup.find(name) != mAssetLookup.end()) {
            return nullptr;
        }
        AssetID id;
        if (mFreeIDs.size()) {
            AssetID id = mFreeIDs.back();
            mFreeIDs.pop_back();
            mAssetLookup[name] = id;
            mAssetRegistry[id] = AssetRegistryEntry{ .mID=id, .mRequestedLoad=true /*Already loaded*/};
            mAssetRefCounts[id]->store(0); // 1?
            // Retain pointer stability by replacing previous asset directly
            *mAssets[id] = T(name, id);
            mLoadedAssets[id]->store(true);
            return getAssetHandle(id);
        }
        else {
            return getAssetHandle(registerAsset(name, ""));
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

    virtual AssetType getAssetType() const = 0;
    virtual bool saveAsset(AssetID assetId) = 0;

private:
    void loadAssetAsync(const AssetRegistryEntry& assetEntry) {
        AssetLoader::getInstance().requestAssetLoad(getAssetLoadFunc(), assetEntry.mID, mAssets[assetEntry.mID].get(), assetEntry.mFilePath, mLoadedAssets[assetEntry.mID].get());
    }

protected:
    virtual AssetLoadFunc getAssetLoadFunc() = 0;

    // ==================================================================
    // Asset friend functions
    // ==================================================================
    void aquireAssetHandle(AssetID id, AssetHandle<T>& handle) {
        assert(!handle.isValid());
        handle.mAssetID = id;
        handle.mAssetType = getAssetType();
        ++(*mAssetRefCounts[handle.mAssetID]);
        if (mLoadedAssets[handle.mAssetID]->load()) {
            handle.mAssetName = mAssets[id]->getName();
            handle.mLoadedAsset = mAssets[id].get();
            return;
        }
        { // This should be fairly rare
            mBeginLoadAssetMutex.lock(); // LOCK
            if (!mAssetRegistry[handle.mAssetID].mRequestedLoad) {
                mAssetRegistry[handle.mAssetID].mRequestedLoad = true;
                mBeginLoadAssetMutex.unlock(); // UNLOCK
                loadAssetAsync(mAssetRegistry[handle.mAssetID]);
            }
            else {
                mBeginLoadAssetMutex.unlock(); // UNLOCK
            }
        }
    }
    void releaseAssetHandle(AssetHandle<T>& handle) {
        if (handle.isValid()) {
            --(*mAssetRefCounts[handle.mAssetID]);
            handle.mAssetName = {};
            handle.mAssetID = INVALID_ASSET_ID;
            handle.mLoadedAsset = nullptr;
        }
    }
    void pollAsset(AssetHandle<T>& handle) {
        if (mLoadedAssets[handle.mAssetID]->load()) {
            handle.mLoadedAsset = mAssets[handle.mAssetID].get();
        }
    }

    // ==================================================================
    // Data
    // ==================================================================
    std::map<StrToken, AssetID> mAssetLookup;
    std::vector<AssetRegistryEntry> mAssetRegistry;
    std::vector<std::unique_ptr<T>> mAssets;
    std::vector<std::unique_ptr<ExclusiveCacheLine<std::atomic_int>>> mAssetRefCounts;
    std::vector<std::unique_ptr<ExclusiveCacheLine<std::atomic_bool>>> mLoadedAssets;
    std::vector<AssetID> mFreeIDs;

    std::mutex mBeginLoadAssetMutex;

    inline static std::unique_ptr<IAssetRepository<T>> sInstance;
    // mDirtyAssets?
};

// Helper for derived classes of IAssetRepository
#define ASSET_REPOSITORY_COMMON_CODE(className, assetDef, assetType) \
public: \
    using IAssetRepository<assetDef>::IAssetRepository; \
    static void initInstance(vio::IOManager& ioManager) { \
        sInstance = std::make_unique<className>(ioManager); \
    } \
    inline static className& get() { \
        assert(sInstance); \
        return (className&)*sInstance; \
    } \
    AssetType getAssetType() const override { return assetType; }


class AssetHandleBase {
public:
    AssetHandleBase() = default;
    virtual ~AssetHandleBase() = default;

    AssetID getAssetID() const { return mAssetID; }

    bool isValid() const { return mAssetName.isValid(); }
    virtual bool isLoaded() = 0;

    AssetDescriptor getDescriptor() const { return AssetDescriptor{.id=mAssetID, .assetType=mAssetType}; }

protected:
    StrToken mAssetName;
    AssetID mAssetID = INVALID_ASSET_ID;
    AssetType mAssetType = AssetType::COUNT;
};

template <IsAssetType T>
class AssetHandle : public AssetHandleBase {
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

    // AssetHandleBase interface
    bool isLoaded() override {
        if (mLoadedAsset != nullptr) return true;
        IAssetRepository<T>::getInstance().pollAsset(*this);
        return mLoadedAsset != nullptr;
    }

    // Returns nullptr if the asset is loading
    const T* tryGetAsset() {
        assert(isValid());
        if (mLoadedAsset) return mLoadedAsset;
        IAssetRepository<T>::getInstance().pollAsset(*this);
        return mLoadedAsset;
    }

    T* editorTryGetMutableAsset() { return const_cast<T*>(tryGetAsset()); }

protected:
    const T* mLoadedAsset = nullptr;
};

template <IsAssetType T>
AssetHandlePtr<T> IAssetRepository<T>::getAssetHandle(StrToken assetName) {
    return getAssetHandle(mAssetLookup.at(assetName));
}

template <IsAssetType T>
AssetHandlePtr<T> IAssetRepository<T>::getAssetHandle(AssetID id) {
    AssetHandlePtr<T> handle = std::make_shared<AssetHandle<T>>();
    aquireAssetHandle(id, *handle);
    return handle;
}

class AssetHandleBundle {
public:
    inline bool hasAssetHandle(AssetDescriptor desc) const {
        return mContainedAssetDescriptors.find(desc) != mContainedAssetDescriptors.end();
    }
    inline bool hasAssetHandle(AssetID id, AssetType assetType) const {
        return mContainedAssetDescriptors.find(AssetDescriptor{ .id = id, .assetType = assetType }) != mContainedAssetDescriptors.end();
    }
    void addAssetHandle(std::shared_ptr<AssetHandleBase> handle) {
        assert(!hasAssetHandle(handle->getDescriptor()));
        mContainedAssetDescriptors.emplace(handle->getDescriptor());
        if (handle->isLoaded()) {
            ++mLoadedCount;
            mHandles.emplace_back(std::make_pair(true, std::move(handle)));
        }
        else {
            mHandles.emplace_back(std::make_pair(false, std::move(handle)));
        }
    }

    bool areAllAssetsLoaded() {
        if (mLoadedCount == mHandles.size()) return true;
        for (auto&& it : mHandles) {
            if (!it.first) {
                if (it.second->isLoaded()) {
                    it.first = true;
                    ++mLoadedCount;
                }
            }
        };
        return mLoadedCount == mHandles.size();
    }

protected:
    std::set<AssetDescriptor> mContainedAssetDescriptors;
    std::vector<std::pair<bool /*loaded*/, std::shared_ptr<AssetHandleBase>>> mHandles;
    int mLoadedCount = 0;
};