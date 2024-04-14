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