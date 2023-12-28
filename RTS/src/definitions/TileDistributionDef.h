#pragma once

#include "generation/NoiseFunction.hpp"

class TileDistributionDef : public IAsset {
public:
    DEFAULT_ASSET_CONSTRUCTOR(TileDistributionDef);

    f32 spacing = 1.0f;
    f32 probability = 1.0f;
    f32v2 offset = f32v2(0.0f);
    std::unique_ptr<NoiseFunction> distFunc;
};
SERIALIZABLE_IMGUI_CONTROLLED(TileDistributionDef,
    make_field(o.spacing, "spacing"sv),
    make_field(o.probability, "prob"sv),
    make_field(o.offset, "offset"sv),
    make_field(o.distFunc, "dist"sv)
);