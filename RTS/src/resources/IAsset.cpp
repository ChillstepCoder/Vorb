#include "stdafx.h"
#include "IAsset.h"

#include "resources/ResourceManager.h"
#include "resources/asset/AssetHandleBundle.h"

IAsset::IAsset(StrToken name, AssetID id) : mName(name), mID(id) {}

IAsset::~IAsset() = default;

void IAsset::addDependency(std::unique_ptr<AssetHandleBase>&& handle) {
    // Passing null handle is allowed for convenience
    if (!handle) {
        return;
    }
    if (!mDependencies) {
        mDependencies = std::make_unique<AssetHandleBundle>();
    }
    mDependencies->addAssetHandle(std::move(handle));
}

void IAsset::reserveDependencyCount(size_t count)
{
    if (!mDependencies) {
        mDependencies = std::make_unique<AssetHandleBundle>();
    }
    mDependencies->reserveCount(count);
}

StrToken AssetDescriptor::getName() const {
    if (!isValid()) {
        return StrToken();
    }
    return Services::ResourceManager::ref().getAssetRepository(assetType).getMetadata(id).mName;
}

std::filesystem::path AssetDescriptor::getPath() const {
    if (!isValid()) {
        return std::filesystem::path();
    }
    return Services::ResourceManager::ref().getAssetRepository(assetType).getMetadata(id).mFilePath.getStdPath();
}

IAssetRepositoryBase* AssetDescriptor::getRepo() const {
    if (!isValid()) {
        return nullptr;
    }
    return &Services::ResourceManager::ref().getAssetRepository(assetType);
}
