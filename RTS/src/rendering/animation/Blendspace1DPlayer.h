#pragma once

#include "definitions/AnimationDef.h"
#include "Blendspace1DPlayerNode.h"
#include "rendering/model/skeletal/AnimVariableFloatBinding.h"

struct AnimVariables;
class Blendspace1DDef;

struct AnimBlendPair {
    // If weight1 is 1.0f, then anim2 is null
    bool hasBoth() const { return weight0 != 1.0f; }

    AssetRawPtr<AnimationDef> anim0;
    AssetRawPtr<AnimationDef> anim1;
    f32 weight0 = 1.0f; // Weight2 is 1 - weight1
};

// Does not store any reference to the Blendspace1DDef,
// asset handle should be tracked by owner such as AnimMachine
class Blendspace1DPlayer {
public:
    friend class Blendspace1DEditorViewportPanel;
    Blendspace1DPlayer() = default;
    Blendspace1DPlayer(const Blendspace1DDef& blendspaceDef);

    void resetSyncAlpha() { mSyncAlpha = 0.0f; }
    // [0, 1]
    void setSyncAlpha(f32 newAlpha) { mSyncAlpha = newAlpha; }

    AnimSampleBlendDataPair updateAndGetBlendData(const AnimVariables& inputs, f32 elapsedSec);
    // Returns valid anim samples with a manual input
    AnimSampleBlendDataPair updateAndGetBlendData(f32 x, f32 elapsedSec);

    // Simply get pair blend with no time or update
    AnimBlendPair getBlendPair(f32 x, OUT f32& outAnimSpeed) const;
    f32 getX() const { return mX; }
    bool isValid() const { return mNodes.size() > 0; }

private:
    // TODO: will be invalidated if Blendspace1DDef is modified in editor
    std::span<const Blendspace1DPlayerNode> mNodes;
    AnimVariableFloatBinding mInputBinding;
    f32 mSyncAlpha = 0.0f;
    f32 mMaxAlphaChangeSpeed = 0.0f;
    f32 mX = 0.0f;
    f32 mSpeedWarpLess = 0.0f;
    f32 mSpeedWarpGreater = 1.0f;
};
