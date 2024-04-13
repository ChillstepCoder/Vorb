#pragma once

#include "resources/asset/AssetType.h"

class SoftAssetReference {
public:
    SoftAssetReference() = default;
    SoftAssetReference(AssetType assetType) : assetType(assetType) {};
    SoftAssetReference(AssetType assetType, StrToken name) : assetType(assetType), name(name) {};

    bool isValid() const { return name.isValid(); }
    void toString(OUT char* outStr, OUT ui32* outLength) const { name.toString(outStr, outLength); }
    nString toString() const { return name.toString(); }
    AssetHandleBasePtr getAssetHandleBase() const;
    template<typename T>
    AssetHandlePtr<T> getAssetHandle() const {
        assert(T::ASSET_TYPE == assetType);
        AssetHandleBasePtr base = getAssetHandleBase();
        return static_unique_pointer_cast<AssetHandle<T>>(std::move(base));
    }
    AssetID getAssetID() const;
    AssetDescriptor getAssetDescriptor() const { return AssetDescriptor{ .id = getAssetID(), .assetType = assetType, }; }
    
    StrToken name;
    AssetType assetType = AssetType::NONE;
};

namespace ImguiUtil {
    bool updateAndRenderSoftAssetReference(const char* label, SoftAssetReference& assetRef);
}