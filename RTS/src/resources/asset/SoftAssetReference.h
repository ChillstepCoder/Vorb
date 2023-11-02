#pragma once

#include "resources/asset/AssetType.h"

class SoftAssetReference {
public:
    SoftAssetReference(AssetType assetType) : assetType(assetType) {};

    bool isValid() const { return name.isValid(); }

    StrToken name;
    const AssetType assetType;
};

namespace ImguiUtil {
    bool updateAndRenderSoftAssetReference(SoftAssetReference& ref);
}