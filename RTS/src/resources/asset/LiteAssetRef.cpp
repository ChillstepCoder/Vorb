#include "stdafx.h"
#include "LiteAssetRef.h"

#include "resources/ResourceManager.h"

StrToken LiteAssetRefBase::getAssetNameInternal(AssetType type) const {
    if (!isValid()) [[unlikely]] {
        return StrToken();
    }
    IAssetRepositoryBase& repo = ResourceManager::get().getAssetRepository(type);
    return repo.getAssetRegistry().at(mId).mName;
}

void LiteAssetRefBase::setAssetNameInternal(StrToken name, AssetType type) {
    if (name.isValid()) {
        IAssetRepositoryBase& repo = ResourceManager::get().getAssetRepository(type);
        mId = repo.getAssetID(name);
    } else {
        mId = INVALID_ASSET_ID;
    }
}

bool LiteAssetRefBase::updateAndRenderImguiInternal(const char* label, AssetType type, AssetFilterFunc filterFunc)  {
    return ImguiUtil::updateAndRenderAssetReference(label, *this, type, filterFunc);
}

IAsset& LiteAssetRefBase::getLoadedOrUnloadedAssetInternal(AssetType type) const {
    return ResourceManager::get().getAssetRepository(type).getLoadedOrUnloadedIAsset(mId);
}

IAsset& LiteAssetRefBase::getLoadedAssetInternal(AssetType type) const {
    return ResourceManager::get().getAssetRepository(type).getLoadedIAsset(mId);
}

AssetHandleBasePtr LiteAssetRefBase::getAssetHandleBaseInternal(AssetType type) const {
    return ResourceManager::get().getAssetRepository(type).getAssetHandleBase(mId);
}
