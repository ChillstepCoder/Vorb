#pragma once

#include "definitions/AnimationDef.h"

struct Blendspace1DPlayerNode {
    f32 x = 0.0f;
    AssetID animId = INVALID_ASSET_ID;
};

struct AnimBlendPair {
    // If weight1 is 1.0f, then anim2 is null
    bool hasBoth() const { return weight0 != 1.0f; }

    // TODO: AssetPtr<T> (Wrapper for 4 byte index which does a direct asset lookup, doesn't incref)
    AssetRawPtr<AnimationDef> anim0;
    AssetRawPtr<AnimationDef> anim1;
    f32 weight0 = 1.0f; // Weight2 is 1 - weight1
};

class Blendspace1DPlayer {
public:
    Blendspace1DPlayer() = default;
    Blendspace1DPlayer(std::span<const Blendspace1DPlayerNode> inNodes);
    bool isValid() const { return mNodes != nullptr; }

    AnimBlendPair getBlendPair(f32 x) const;

    // TODO: This eliminates the bitArray so can be more efficient than AssetHandleBundle, use this there?
    bool areAllAssetsLoaded() const {
        if (mLoadedCount == mNumNodes) [[likely]] { return true; }
        mLoadedCount = 0;
        for (ui32 i = 0; i < mNumNodes; ++i) {
            if (mAnimAssetHandles[i]->isLoaded()) {
                ++mLoadedCount;
            }
            else {
                // We don't need to check every one if one fails
                return false;
            }
        };
        assert(mLoadedCount == mNumNodes);
        return true;
    }

    std::unique_ptr<AssetHandlePtr<AnimationDef>[]> mAnimAssetHandles; // size == numNodes
    std::unique_ptr<Blendspace1DPlayerNode[]> mNodes;
    ui32 mNumNodes = 0;
    f32 mTime = 0.0f;
    f32 mSyncTime = 0.0f;
    bool mAllAssetsLoaded = false;
private:
    mutable i16 mLoadedCount = 0;
};