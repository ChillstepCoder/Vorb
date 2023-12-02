#pragma once

#include "resources/asset/AssetType.h"

class SoftAssetReference {
public:
    SoftAssetReference(AssetType assetType) : assetType(assetType) {};

    bool isValid() const { return name.isValid(); }
    void toString(OUT char* outStr, OUT ui32* outLength) const { name.toString(outStr, outLength); }
    nString toString() const { return name.toString(); }
    AssetHandleBasePtr getAssetHandle() const;
    AssetID getAssetID() const;
    
    StrToken name;
    const AssetType assetType;
};

namespace ImguiUtil {
    bool updateAndRenderSoftAssetReference(const char* label, SoftAssetReference& assetRef);
}