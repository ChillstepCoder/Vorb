#pragma once

#include "rendering/material/MaterialData.h"
#include "definitions/rendering/TextureDef.h"

namespace ImguiAssetThumbnails {
    template <typename T>
    std::function<void(AssetID, f32v2)> getThumbnailFunction() {
        return nullptr;
    }


    // Specialize for each asset type
    template<>
    std::function<void(AssetID, f32v2)> getThumbnailFunction<MaterialDef>();
    template<>
    std::function<void(AssetID, f32v2)> getThumbnailFunction<TextureDef>();

    static_assert(e_count(AssetType) == 17);
};
