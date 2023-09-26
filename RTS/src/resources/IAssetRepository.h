#pragma once

#include "util/ExclusiveCacheLine.h"

#include "resources/AssetLoadTask.h"
#include "resources/AssetLoader.h"

#include "resources/asset/AssetHandleBundle.h"

class AssetLoader;

DECL_VIO(class IOManager);

struct AssetRegistryEntry {
    StrToken mName;
    vio::Path mFilePath;
    AssetID mID = INVALID_ASSET_ID; // TODO: unneeded
    bool mRequestedLoad = false;
    //bool mFinishedLoading = false; // Or refcount needed to ensure we dont destroy this while
    // it is being loaded once we implement deallocation of assets
};

class IAssetRepositoryBase {
public:
    virtual ~IAssetRepositoryBase() = default;

    AssetHandleBasePtr getAssetHandleBase(AssetID id);
    AssetHandleBasePtr getAssetHandleBase(StrToken assetName);

    virtual AssetType getAssetType() const = 0;

    // Called after every asset in the game has been registered
    virtual void onAllAssetTypesRegistered() {};
protected:
    IAssetRepositoryBase(vio::IOManager& ioManager) : mIoManager(ioManager) {}

    // Virtual interfaces
    virtual void loadAssetAsync(const AssetRegistryEntry& assetEntry) = 0;
    virtual void fillAsset(AssetHandleBase& handle) = 0;
    virtual AssetHandleBasePtr makeAssetHandle() = 0;
    virtual AssetID getAssetID(StrToken assetName) const = 0;

    // ==================================================================
    // Asset friend functions
    // ==================================================================
    void aquireAssetHandle(StrToken name, AssetHandleBase& handle) {
        aquireAssetHandle(getAssetID(name), handle);
    }
    void aquireAssetHandle(AssetID id, AssetHandleBase& handle) {
        assert(!handle.isValid());
        assert(id < mAssetRegistry.size());
        handle.mAssetID = id;
        handle.mAssetType = getAssetType();
        handle.mAssetName = mAssetRegistry[id].mName;
        ++(*mAssetRefCounts[handle.mAssetID]);
        if (mLoadedAssets[handle.mAssetID]->load()) {
            fillAsset(handle);
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
    void releaseAssetHandle(AssetHandleBase& handle) {
        if (handle.isValid()) {
            --(*mAssetRefCounts[handle.mAssetID]);
            handle.mAssetName = {};
            handle.mAssetID = INVALID_ASSET_ID;
            // handle.mLoadedAsset = nullptr; // Not needed just let it dangle
        }
    }
    void pollAsset(AssetHandleBase& handle) {
        if (mLoadedAssets[handle.mAssetID]->load()) {
            fillAsset(handle);
        }
    }

    bool saveAssetContents(const IAsset& asset, const vio::Path& path, const char* fileContents, size_t sizeBytes);
    // TODO: We could use a threadlocal global string so we dont realloc?
    nString readFileToString(const vio::Path& path);

    vio::IOManager& mIoManager;
    std::map<StrToken, AssetID> mAssetLookup;
    std::vector<AssetRegistryEntry> mAssetRegistry;
    std::vector<std::unique_ptr<ExclusiveCacheLine<std::atomic_int>>> mAssetRefCounts;
    std::vector<std::unique_ptr<ExclusiveCacheLine<std::atomic_bool>>> mLoadedAssets;
    std::vector<AssetID> mFreeIDs;

    std::mutex mBeginLoadAssetMutex;
};

template <IsAssetType T>
class IAssetRepository : public IAssetRepositoryBase {
public:
    friend class AssetHandle<T>;

    IAssetRepository(vio::IOManager& ioManager) : IAssetRepositoryBase(ioManager) { initInternal(); }
    virtual ~IAssetRepository() = default;

    inline static IAssetRepository<T>& getInstance() {
        assert(sInstance);
        return *sInstance;
    }

    // ==================================================================
    // Public interface
    // ==================================================================
    // If return true, break
    void forEachLoadedAsset(std::function<bool(IAssetRepository<T>&, T&)> func) {
        for (AssetID id = 0; id < mAssets.size(); ++id) {
            if (mLoadedAssets[id]->load()) {
                if (func(*this, *mAssets[id])) return;
            }
        };
    }
    // If return true, break
    void forEachRegisteredAsset(std::function<bool(T*, const AssetRegistryEntry& entry)> func) {
        for (AssetID id = 0; id < mAssets.size(); ++id) {
            if (mLoadedAssets[id]->load()) {
                if (func(mAssets[id].get(), mAssetRegistry[id])) return;
            }
            else {
                if (func(nullptr, mAssetRegistry[id])) return;
            }
        };
    }

    vio::Path getAssetFilePath(AssetID id) const { return mAssetRegistry[id].mFilePath; }
    vio::Path getAssetFilePath(StrToken name) const { return mAssetRegistry[mAssetLookup.at(name)].mFilePath; }
    // TODO: Handle deleting old file?
    void changeAssetFilePath(AssetID id, const vio::Path& newPath) { mAssetRegistry[id].mFilePath = newPath; }

    // Will begin a lazy async load if asset is not loaded
    AssetDescriptor getAssetDescriptor(StrToken assetName) {
        return AssetDescriptor{ .id = getAssetID(assetName), .assetType = getAssetType() };
    }
    AssetHandlePtr<T> getAssetHandle(StrToken assetName) {
        return std::static_pointer_cast<AssetHandle<T>>(getAssetHandleBase(assetName));
    }
    AssetHandlePtr<T> getAssetHandle(AssetID id) {
        return std::static_pointer_cast<AssetHandle<T>>(getAssetHandleBase(id));
    }
    // Note that if you do not have a handle, this could become invalid!
    T* tryGetLoadedAsset(StrToken assetName) {
        return tryGetLoadedAsset(getAssetID(assetName));
    }
    T* tryGetLoadedAsset(AssetID id) {
        if (mLoadedAssets[id]) {
            return mAssets[id].get();
        }
        return nullptr;
    }
    T& getLoadedAsset(StrToken name) {
        return getLoadedAsset(getAssetID(name));
    }
    T& getLoadedAsset(AssetID id) {
        assert(mLoadedAssets[id]);
        return *mAssets[id].get();
    }
    // Some assets are valid without being loaded as they have minimal definitions that can be loaded on register
    T& getLoadedOrUnloadedAsset(StrToken name) {
        return *mAssets[getAssetID(name)];
    }
    T& getLoadedOrUnloadedAsset(AssetID id) {
        return *mAssets[id];
    }
    inline bool isAssetLoaded(AssetID id) { return mLoadedAssets[id]; }

    AssetID getAssetID(StrToken assetName) const override { return mAssetLookup.at(assetName); }
    AssetID registerAsset(const vio::Path& filePath) {
        // TODO remove string copy
        return registerAsset(StrToken(filePath.getFileNameNoExtension()), filePath);
    }

    // ALL assets must be registered before any are loaded, else we will have race conditions
    AssetID registerAsset(StrToken name, const vio::Path& filePath) {
        AssetID id = mAssets.size();
        if (isAssetRegistered(name)) {
            panic("Asset name {} already registered - {}", name.toString().c_str(), filePath.getCString());
        }
        mAssetLookup[name] = id;
        mAssetRegistry.emplace_back(AssetRegistryEntry{ .mName=name, .mFilePath=filePath, .mID=id});
        mAssetRefCounts.emplace_back(std::make_unique<ExclusiveCacheLine<std::atomic_int>>(0));
        mAssets.emplace_back(std::make_unique<T>(name, id));
        mLoadedAssets.emplace_back(std::make_unique<ExclusiveCacheLine<std::atomic_bool>>(false));
        onRegisteredAsset(id);
        return id;
    }
    inline bool isAssetRegistered(StrToken name) const {
        return mAssetLookup.find(name) != mAssetLookup.end();
    }
    size_t getNumRegisteredAssets() const { return mAssets.size(); }

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

    virtual bool saveAsset(AssetID assetId) = 0;

private:
    void loadAssetAsync(const AssetRegistryEntry& assetEntry) override {
        AssetLoader::getInstance().requestAssetLoad(getAssetLoadFunc(), getAssetLoadRenderProcessFunc(), assetEntry.mID, mAssets[assetEntry.mID].get(), assetEntry.mFilePath, mLoadedAssets[assetEntry.mID].get(), getUserData(assetEntry.mID));
    }

protected:
    void loadAssetDependencies(AssetID id, AssetLoadFunc loadFunc, AssetLoadFunc renderPostFunc) {
        const AssetRegistryEntry& assetEntry = mAssetRegistry[id];
        AssetHandleBundle* dependencies = mAssets[id]->getDependencies();
        if (!dependencies) panic("Tried to add null dependencies to loadAssetDependencies");
        AssetLoader::getInstance().requestAssetLoadWithDependencies(loadFunc, renderPostFunc, id, mAssets[assetEntry.mID].get(), assetEntry.mFilePath, mLoadedAssets[assetEntry.mID].get(), getUserData(), dependencies);
    }

    virtual void initInternal() {};
    virtual AssetLoadFunc getAssetLoadFunc() = 0;
    virtual AssetLoadFunc getAssetLoadRenderProcessFunc() {
        return nullptr;
    }
    virtual void onRegisteredAsset(AssetID id) {};
    virtual std::any getUserData(AssetID id) { return nullptr; }

    void fillAsset(AssetHandleBase& handle) override {
        AssetHandle<T>& typedHandle = (AssetHandle<T>&)handle;
        typedHandle.mLoadedAsset = mAssets[typedHandle.mAssetID].get();
    }
    AssetHandleBasePtr makeAssetHandle() {
        return std::make_shared<AssetHandle<T>>();
    }

    // ==================================================================
    // Asset Data
    // ==================================================================
    std::vector<std::unique_ptr<T>> mAssets;

    inline static std::unique_ptr<IAssetRepository<T>> sInstance;
    // mDirtyAssets?
};

#define ASSET_REPOSITORY_COMMON_CODE_NO_CONSTRUCTOR(className, assetDef, assetType) \
public: \
 static void initInstance(vio::IOManager& ioManager) { \
        sInstance = std::make_unique<className>(ioManager); \
    } \
    inline static className& get() { \
        assert(sInstance); \
        return (className&)*sInstance; \
    } \
    AssetType getAssetType() const override { return assetType; }

// Helper for derived classes of IAssetRepository
#define ASSET_REPOSITORY_COMMON_CODE(className, assetDef, assetType) \
public: \
    using IAssetRepository<assetDef>::IAssetRepository; \
    ASSET_REPOSITORY_COMMON_CODE_NO_CONSTRUCTOR(className, assetDef, assetType)
   
AssetHandleBasePtr IAssetRepositoryBase::getAssetHandleBase(StrToken assetName) {
    return getAssetHandleBase(mAssetLookup.at(assetName));
}

AssetHandleBasePtr IAssetRepositoryBase::getAssetHandleBase(AssetID id) {
    AssetHandleBasePtr handle = makeAssetHandle();
    aquireAssetHandle(id, *handle);
    return handle;
}

template<typename T>
const T& AssetHandleBundle::getLoadedAsset(StrToken assetName) {
    return static_cast<AssetHandle<T>*>(tryGetAssetHandle(IAssetRepository<T>::getInstance().getAssetDescriptor(assetName)))->getLoadedAsset();
}

template <typename T>
const T* AssetHandle<T>::tryGetAsset() const {
    assert(isValid());
    if (mLoadedAsset) [[likely]] { return mLoadedAsset; }
    IAssetRepository<T>::getInstance().pollAsset(const_cast<AssetHandle<T>&>(*this));
    return mLoadedAsset;
}

template <typename T>
bool AssetHandle<T>::isLoaded() const {
    if (mLoadedAsset != nullptr) [[likely]] { return true; }
    IAssetRepository<T>::getInstance().pollAsset(const_cast<AssetHandle<T>&>(*this));
    return mLoadedAsset != nullptr;
}

template <typename T>
void AssetHandle<T>::release() {
    IAssetRepository<T>::getInstance().releaseAssetHandle(*this);
}

template <typename T>
void AssetHandle<T>::aquire(AssetID id) {
    IAssetRepository<T>::getInstance().aquireAssetHandle(id, *this);
}

template <typename T>
void AssetHandle<T>::aquire(StrToken name) {
    IAssetRepository<T>::getInstance().aquireAssetHandle(name, *this);
}

template <typename T>
const T& AssetHandle<T>::getLoadedAsset() const {
    assert(mLoadedAsset);
    return *mLoadedAsset;
}

namespace AssetUtil {
    // Useful when you want to put a bunch of assets into a bundle and cache the asset
    // so once they are all loaded you don't need to query them
    // MAKE SURE THAT THE BUNDLE IS FULLY LOADED BEFORE USING THE RETURNED ASSET UNLESS YOU KNOW IT IS VALID DEF DATA
    template <typename T>
    [[nodiscard]] T* addAssetToBundleAndGetUnloaded(AssetHandleBundle& bundle, StrToken assetName) {
        bundle.addAssetHandle(IAssetRepository<T>::getInstance().getAssetHandle(assetName));
        return &IAssetRepository<T>::getInstance().getLoadedOrUnloadedAsset(assetName);
    }
}