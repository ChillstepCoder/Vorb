#include "stdafx.h"
#include "AssetIdReference.h"

#include "resources/ResourceManager.h"

inline StrToken AssetIdReferenceBase::getAssetName(AssetType type) {
    if (!isValid()) [[unlikely]] {
        return StrToken();
    }
    IAssetRepositoryBase& repo = ResourceManager::get().getAssetRepository(type);
    return repo.getAssetRegistry().at(id).mName;
}

bool AssetIdReferenceBase::updateAndRenderImguiInternal(const char* label, AssetType type, AssetFilterFunc filterFunc) {
    // Share soft asset reference logic for now
    SoftAssetReference ref(type, isValid() ? getAssetName(type) : StrToken());
    bool changed = ImguiUtil::updateAndRenderSoftAssetReference(label, ref, filterFunc);
    if (changed) {
        id = ref.getAssetID();
    }
    return changed;
}
