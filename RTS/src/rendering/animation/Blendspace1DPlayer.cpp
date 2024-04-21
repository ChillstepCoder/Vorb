#include "stdafx.h"
#include "Blendspace1DPlayer.h"

#include "resources/AnimationRepository.h"

Blendspace1DPlayer::Blendspace1DPlayer(std::span<const Blendspace1DPlayerNode> inNodes) {
    mNodes = std::make_unique<Blendspace1DPlayerNode[]>(inNodes.size());
    memcpy(mNodes.get(), inNodes.data(), inNodes.size() * sizeof(Blendspace1DPlayerNode));
    mNumNodes = inNodes.size();
    mAnimAssetHandles = std::make_unique<AssetHandlePtr<AnimationDef>[]>(mNumNodes);
    for (ui32 i = 0; i < mNumNodes; ++i) {
        assert(inNodes[i].animId != INVALID_ASSET_ID);
        mAnimAssetHandles[i] = AnimationRepository::get().getAssetHandle(inNodes[i].animId);
    }
}

AnimBlendPair Blendspace1DPlayer::getBlendPair(f32 x) const {
    assert(mNumNodes > 0);

    // Before first node
    if (x <= mNodes[0].x) {
        return { mNodes[0].animId, INVALID_ASSET_ID, 1.0f };
    }

    for (size_t i = 0; i < mNumNodes - 1; ++i) {
        if (x >= mNodes[i].x && x < mNodes[i + 1].x) {
            const f32 t = (x - mNodes[i].x) / (mNodes[i + 1].x - mNodes[i].x);
            AnimBlendPair rv{ mNodes[i].animId, mNodes[i + 1].animId, 1.0f - t };
            if (rv.weight0 == 0.0f) {
                // Only return second anim
                rv.anim0 = rv.anim1;
                rv.anim1 = INVALID_ASSET_ID;
                rv.weight0 = 1.0f;
            }
            else if (rv.weight0 == 1.0f) {
                rv.anim1 = INVALID_ASSET_ID;
            }
            return rv;
        }
    }
    // After last node
    return { mNodes[mNumNodes - 1].animId, INVALID_ASSET_ID, 1.0f };
}
