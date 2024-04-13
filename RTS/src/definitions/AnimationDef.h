#pragma once

#include <ozz/animation/runtime/animation.h>

class AnimationDef : public IAsset {
public:
    DEFAULT_ASSET_CONSTRUCTOR(AnimationDef, AssetType::Animation);

    ozz::animation::Animation mAnimation;
    SoftAssetReference rigDef = SoftAssetReference(AssetType::Rig);
};
SERIALIZABLE_IMGUI_CONTROLLED(AnimationDef,
    make_field(o.rigDef, "rig"sv)
)