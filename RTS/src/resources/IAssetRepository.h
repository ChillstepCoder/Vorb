#pragma once

#include "resources/IAsset.h"

DECL_VIO(class IOManager);

class IAssetRepositoryBase {
public:
    IAssetRepositoryBase(vio::IOManager& ioManager) : mIoManager(ioManager) {}
    virtual ~IAssetRepositoryBase() = default;

protected:
    vio::IOManager& mIoManager;
    bool saveAssetContents(const IAsset& asset, const char* fileContents, size_t sizeBytes);
};

template <IsAssetType T>
class IAssetRepository : public IAssetRepositoryBase {
public:
    IAssetRepository(vio::IOManager& ioManager) : IAssetRepositoryBase(ioManager) {}
    virtual ~IAssetRepository() = default;

    const T& getAsset(AssetID id) const { return mAssets[id]; }
    const T& getAsset(StrToken itemName) const {
        auto&& it = mAssetLookup.find(itemName);
        assert(it != mAssetLookup.end());
        return mAssets[it->second];
    }
    const std::vector<T>& getAllAssets() const { return mAssets; }
    std::vector<T>& getAllAssetsMutable() { return mAssets; }
    const std::map<StrToken, AssetID>& getAssetNames() const { return mAssetLookup; }

    T* tryAddNewAsset(StrToken name) {
        if (mAssetLookup.find(name) != mAssetLookup.end()) {
            return nullptr;
        }
        AssetID id;
        if (mFreeIDs.size()) {
            AssetID id = mFreeIDs.back();
            mFreeIDs.pop_back();
            mAssetLookup[name] = id;
            mAssets[id] = T(name, id);
            return &mAssets[id];
        }
        else {
            AssetID id = mAssets.size();
            mAssetLookup[name] = id;
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
        mAssetLookup.erase(mAssets[id].getName());
        // TODO: Remove file
    }

protected:
    std::map<StrToken, AssetID> mAssetLookup;
    std::vector<T> mAssets;
    std::vector<AssetID> mFreeIDs;
};

