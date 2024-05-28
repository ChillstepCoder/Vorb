#pragma once

#include "generation/NoiseFunction.hpp"
#include "util/BitArray.h"

class TileDistributionDef : public IAsset {
public:
    DEFAULT_ASSET_CONSTRUCTOR(TileDistributionDef, AssetType::TileDistribution);

    i32 spacing = 1;
    i32 minDistance = 1;
    f32 probability = 1.0f;
    f32v2 offset = f32v2(0.0f);
    std::unique_ptr<NoiseFunction> distFunc;
    BitArray precalculatedDistribution;
};
SERIALIZABLE_IMGUI_CONTROLLED(TileDistributionDef,
    make_field(o.spacing, "spacing"sv),
    make_field(o.minDistance, "min_dist"sv),
    make_field(o.probability, "prob"sv),
    make_field(o.offset, "offset"sv),
    make_field(o.distFunc, "dist"sv)
);
