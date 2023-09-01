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
    const T& getAsset(const nString& itemName) const {
        auto&& it = mAssetLookup.find(itemName);
        assert(it != mAssetLookup.end());
        return mAssets[it->second];
    }
    const std::vector<T>& getAllAssets() const { return mAssets; }
    std::vector<T>& getAllAssetsMutable() { return mAssets; }
    const std::map<nString, AssetID>& getAssetNames() const { return mAssetLookup; }

    T* tryAddNewAsset(const nString& name) {
        if (mAssetLookup.find(name) != mAssetLookup.end()) {
            return nullptr;
        }

        AssetID id = mAssets.size();
        mAssetLookup[name] = id;
        T& newAsset = mAssets.emplace_back(name, id);
        return &newAsset;
    }


protected:
    std::map<nString, AssetID> mAssetLookup; // TODO: StrToken?
    std::vector<T> mAssets;
};

