#pragma once

#include "resources/IAssetRepository.h"

// For simple "always loaded" data assets
template <typename T, AssetType ASSET_TYPE, StringLiteral EXTENSION>
class DataAssetRepository : public IAssetRepository<T>
{
public:
    using IAssetRepository<T>::IAssetRepository;
    AssetType getAssetType() const override {
        return ASSET_TYPE;
    }

    DEFAULT_ASSET_SAVE_FUNC();

    StrToken getAssetExtension() const override { return CStrToken(EXTENSION.value); }
    const char* const getAssetTypeDisplayName() const override { return EXTENSION.value; }

protected:

    void onRegisteredAsset(AssetID id) override {
        T& def = *this->mAssets[id];
        const vio::Path& filePath = this->mAssetRegistry[id].mFilePath;
        YmlSerializer::readFileData(this->readFileToString(filePath), def);
        this->mLoadedAssets[id]->store(true);
        this->fixupAsset(id);
    }

    AssetLoadFunc getAssetLoadFunc() override { panic("Data asset repo tried to call Load Func"); return nullptr; }
};

