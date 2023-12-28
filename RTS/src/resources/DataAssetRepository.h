#pragma once

#include "resources/IAssetRepository.h"

struct conststr
{
    const char* const p;
    template<std::size_t N>
    constexpr conststr(const char(&a)[N]) : p(a)/*, sz(N - 1) */ {}
};

// For simple "always loaded" data assets
template <typename T, AssetType ASSET_TYPE, conststr EXTENSION>
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

    StrToken getAssetExtension() const override { return CStrToken(EXTENSION.p); }
    const char* const getAssetTypeDisplayName() const override { return EXTENSION.p; }

protected:

    void onRegisteredAsset(AssetID id) override {
        T& def = *mAssets[id];
        const vio::Path& filePath = mAssetRegistry[id].mFilePath;
        YmlSerializer::readFileData(readFileToString(filePath), def);
    }

    AssetLoadFunc getAssetLoadFunc() override { panic("Data asset repo tried to call Load Func"); return nullptr; }
};

