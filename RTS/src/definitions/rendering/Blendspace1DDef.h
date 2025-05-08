#pragma once

class AnimationDef;
#include "rendering/animation/Blendspace1DPlayerNode.h"
#include "rendering/model/skeletal/AnimVariableFloatBinding.h"

struct Blendpsace1DDefNode {
    AnimationAssetRef animation;
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
    RigAssetRef rigDef;
    AnimVariableFloatBindingDef inputBinding;
    f32 maxXChangeSpeed = 0.0f; // How quickly the X value can change, lower = smoother, 0 = instant
    f32 speedWarpFactorLess = 0.0f; // How much speed can reduce by when below left node, 0 = no reduction, 1 = full reduction
    f32 speedWarpFactorGreater = 0.0f; // How much speed can reduce by when above right node, 0 = no reduction, 1 = full reduction

    // For easy blendspace player instantiation
    AnimVariableFloatBinding inputBindingRuntime;
    std::vector<Blendspace1DPlayerNode> playerNodesRuntime;
};
SERIALIZABLE_SIMPLE(Blendspace1DDef,
    make_field(o.nodes, "nodes"sv),
    make_field(o.rigDef, "rig"sv),
    make_field(o.inputBinding, "input"sv),
    make_field(o.maxXChangeSpeed, "max_x_speed"sv),
    make_field(o.speedWarpFactorLess, "speed_warp_less"sv),
    make_field(o.speedWarpFactorGreater, "speed_warp_greater"sv)
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
