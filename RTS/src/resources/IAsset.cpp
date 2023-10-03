#include "stdafx.h"
#include "IAsset.h"

#include "resources/asset/AssetHandleBundle.h"

IAsset::IAsset(StrToken name, AssetID id) : mName(name), mID(id) {}

IAsset::~IAsset() = default;

void IAsset::addDependency(std::shared_ptr<AssetHandleBase> handle) {
    assert(handle);
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
