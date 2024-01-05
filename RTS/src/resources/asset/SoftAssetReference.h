#pragma once

#include "resources/asset/AssetType.h"

class SoftAssetReference {
public:
    SoftAssetReference() = default;
    SoftAssetReference(AssetType assetType) : assetType(assetType) {};

    bool isValid() const { return name.isValid(); }
    void toString(OUT char* outStr, OUT ui32* outLength) const { name.toString(outStr, outLength); }
    nString toString() const { return name.toString(); }
    AssetHandleBasePtr getAssetHandle() const;
    AssetID getAssetID() const;
    AssetDescriptor getAssetDescriptor() const { return AssetDescriptor{ .id = getAssetID(), .assetType = assetType, }; }
    
    StrToken name;
    AssetType assetType = AssetType::NONE;
};

namespace ImguiUtil {
    bool updateAndRenderSoftAssetReference(const char* label, SoftAssetReference& assetRef);
}