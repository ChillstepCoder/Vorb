#pragma once

class AnimationDef;
#include "rendering/animation/Blendspace1DPlayerNode.h"
#include "rendering/model/skeletal/AnimVariableFloatBinding.h"

struct Blendpsace1DDefNode {
    SoftAssetReference animation = SoftAssetReference(AssetType::Animation);
    f32 x = 0.0f;
    i32 editorIndex = -1; // Used for sorting and such
};
SERIALIZABLE_SIMPLE(Blendpsace1DDefNode,
    make_field(o.animation, "anim"sv),
    make_field(o.x, "x"sv)
)

class Blendspace1DDef : public IAsset {
public:
    DEFAULT_ASSET_CONSTRUCTOR(Blendspace1DDef, AssetType::Blendspace1D);
    std::vector<Blendpsace1DDefNode> nodes;
    SoftAssetReference rigDef = SoftAssetReference(AssetType::Rig);
    AnimVariableFloatBindingDef inputBinding;
    // For easy blendspace player instantiation
    AnimVariableFloatBinding inputBindingRuntime;
    std::vector<Blendspace1DPlayerNode> playerNodes;
};
SERIALIZABLE_SIMPLE(Blendspace1DDef,
    make_field(o.nodes, "nodes"sv),
    make_field(o.rigDef, "rig"sv),
    make_field(o.inputBinding, "input"sv)
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
