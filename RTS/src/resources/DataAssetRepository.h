#pragma once

#include "resources/IAssetRepository.h"

// For simple "always loaded" data assets
template <typename T, AssetType ASSET_TYPE, StringLiteral EXTENSION>
class DataAssetRepository : public IAssetRepository<T>
{
public:
    using IAssetRepository<T>::IAssetRepository;
    static void initInstance(vio::IOManager& ioManager) {
        sInstance = std::make_unique<DataAssetRepository>(ioManager);
    }
    inline static DataAssetRepository<T, ASSET_TYPE, EXTENSION>& get() {
        assert(sInstance);
        return (DataAssetRepository&)*sInstance;
    } 
    AssetType getAssetType() const override {
        return ASSET_TYPE;
    }

    DEFAULT_ASSET_SAVE_FUNC();

    StrToken getAssetExtension() const override { return CStrToken(EXTENSION.value); }
    const char* const getAssetTypeDisplayName() const override { return EXTENSION.value; }

protected:

    void onRegisteredAsset(AssetID id) override {
        T& def = *mAssets[id];
        const vio::Path& filePath = mAssetRegistry[id].mFilePath;
        YmlSerializer::readFileData(readFileToString(filePath), def);
    }

    AssetLoadFunc getAssetLoadFunc() override { panic("Data asset repo tried to call Load Func"); return nullptr; }
};

