#include "stdafx.h"
#include "LiteAssetRef.h"

#include "resources/ResourceManager.h"

inline StrToken LiteAssetRefBase::getAssetNameInternal(AssetType type) const {
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
    // Share soft asset reference logic for now
    VariantAssetRef ref(type, isValid() ? getAssetNameInternal(type) : StrToken());
    bool changed = ImguiUtil::updateAndRenderVariantAssetReference(label, ref, filterFunc);
    if (changed) {
        mId = ref.getAssetID();
    }
    return changed;
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
