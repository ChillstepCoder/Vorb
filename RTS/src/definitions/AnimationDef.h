#pragma once

#include <ozz/animation/runtime/animation.h>

class AnimationDef : public IAsset {
public:
    DEFAULT_ASSET_CONSTRUCTOR(AnimationDef, AssetType::Animation);

    ozz::animation::Animation animation;
    SoftAssetReference rigDef = SoftAssetReference(AssetType::Rig);
    bool syncToFeet = false;
    bool isUpperBody = false;
};
SERIALIZABLE_IMGUI_CONTROLLED(AnimationDef,
    make_field(o.rigDef, "rig"sv),
    make_field(o.syncToFeet, "sync_feet"sv),
    make_field(o.isUpperBody, "is_upper"sv)
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
