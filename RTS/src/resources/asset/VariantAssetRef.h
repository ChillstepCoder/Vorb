#pragma once

#include "resources/asset/AssetType.h"

class VariantAssetRef {
public:
    VariantAssetRef() = default;
    VariantAssetRef(AssetType assetType) : assetType(assetType) {};
    VariantAssetRef(AssetType assetType, StrToken name) : assetType(assetType), name(name) {};

    bool isValid() const { return name.isValid(); }
    void invalidate() { name = StrToken(); }
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

    auto operator<=>(const VariantAssetRef&) const = default;
    
    StrToken name;
    AssetType assetType = AssetType::NONE;
};

namespace ImguiUtil {
    bool updateAndRenderVariantAssetReference(const char* label, VariantAssetRef& assetRef, AssetFilterFunc filterFunc = nullptr);
}
