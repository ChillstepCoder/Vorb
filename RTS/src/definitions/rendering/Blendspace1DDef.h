#pragma once

class AnimationDef;

struct Blendpsace1DDefNode {
    SoftAssetReference animation = SoftAssetReference(AssetType::Animation);
    f32 x = 0.0f;
};
SERIALIZABLE_IMGUI_CONTROLLED(Blendpsace1DDefNode,
    make_field(o.animation, "anim"sv),
    make_field(o.x, "x"sv)
)

struct Blendspace1DDef : public IAsset {
public:
    DEFAULT_ASSET_CONSTRUCTOR(Blendspace1DDef, AssetType::Blendspace1D);
    std::vector<Blendpsace1DDefNode> nodes;
    SoftAssetReference rigDef = SoftAssetReference(AssetType::Rig);
};
SERIALIZABLE_IMGUI_CONTROLLED(Blendspace1DDef,
    make_field(o.nodes, "nodes"sv),
    make_field(o.rigDef, "rig"sv)
)

struct Blendspace1DNode {
    AnimationDef* animation = nullptr;
    f32 x = 0.0f;
    // f32 padding
};

struct Blendspace1D {
    std::unique_ptr<Blendspace1DNode[]> nodes;
    ui8 numNodes;
};
