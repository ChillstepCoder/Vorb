#pragma once

#include "definitions/AnimationDef.h"
#include "Blendspace1DPlayerNode.h"

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
    friend class Blendspace1DEditorViewportPanel;
    Blendspace1DPlayer() = default;
    Blendspace1DPlayer(const Blendspace1DDef& blendspaceDef);

    void resetSyncAlpha() {  mSyncAlpha = 0.0f; }
    // [0, 1]
    void setSyncAlpha(f32 newAlpha) { mSyncAlpha = newAlpha; }

    // Returns valid anim samples
    AnimSampleBlendDataPair updateAndGetBlendData(f32 x, f32 elapsedSec);

    // Simply get pair blend with no time or update
    AnimBlendPair getBlendPair(f32 x) const;

    bool isValid() const { return mNodes != nullptr; }

private:
    std::unique_ptr<Blendspace1DPlayerNode[]> mNodes;
    ui32 mNumNodes = 0;
    f32 mSyncAlpha = 0.0f;
};
