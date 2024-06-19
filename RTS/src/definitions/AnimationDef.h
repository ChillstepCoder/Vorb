#pragma once

#include <ozz/animation/runtime/animation.h>

class AnimationDef : public IAsset {
public:
    DEFAULT_ASSET_CONSTRUCTOR(AnimationDef, AssetType::Animation);

    ozz::animation::Animation animation;
    RigAssetRef rigDef;
    bool syncToFeet = false;
    bool isUpperBody = false;
    f32 blendInDuration = 0.2;
    f32 blendOutDuration = 0.2;
};
SERIALIZABLE_IMGUI_CONTROLLED(AnimationDef,
    make_field(o.rigDef, "rig"sv),
    make_field(o.syncToFeet, "sync_feet"sv),
    make_field(o.isUpperBody, "is_upper"sv),
    make_field(o.blendInDuration, "blend_in"sv),
    make_field(o.blendOutDuration, "blend_out"sv)
)

struct AnimSampleBlendData {
    const AnimationDef* anim = nullptr;
    f32 weight = 0.0f;
    f32 animTime = 0.0f;
};

struct AnimSampleBlendDataPair {
    AnimSampleBlendDataPair() : first(), second() {};

    union {
        struct {
            AnimSampleBlendData first;
            AnimSampleBlendData second;
        };
        AnimSampleBlendData arry[2];
    };
    ui8 validCount = 0;

    std::span<AnimSampleBlendData> toSpan() {
        return std::span<AnimSampleBlendData>(&first, validCount);
    }
};
