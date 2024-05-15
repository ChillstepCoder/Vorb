#include "stdafx.h"
#include "Blendspace1DPlayer.h"

#include "resources/AnimationRepository.h"
#include "definitions/rendering/Blendspace1DDef.h"


Blendspace1DPlayer::Blendspace1DPlayer(const Blendspace1DDef& blendspaceDef) {
    assert(blendspaceDef.playerNodesRuntime.size() <= blendspaceDef.nodes.size());
    mInputBinding = blendspaceDef.inputBindingRuntime;
    mNodes = std::span<const Blendspace1DPlayerNode>(blendspaceDef.playerNodesRuntime.data(), blendspaceDef.playerNodesRuntime.size());
    mMaxAlphaChangeSpeed = blendspaceDef.maxXChangeSpeed;
    mSpeedWarpLess = blendspaceDef.speedWarpFactorLess;
    mSpeedWarpGreater = blendspaceDef.speedWarpFactorGreater;
}

AnimSampleBlendDataPair Blendspace1DPlayer::updateAndGetBlendData(const AnimVariables& inputs, f32 elapsedSec) {
    // Get float variable via offset into inputs
    f32 x = *(const f32*)((const ui8*)&inputs + mInputBinding.byteOffset);
    // Scale to [0,1]
    x = (x - mInputBinding.rangeStart) * mInputBinding.inverseRange;
    return updateAndGetBlendData(x, elapsedSec);
}

AnimSampleBlendDataPair Blendspace1DPlayer::updateAndGetBlendData(f32 x, f32 elapsedSec) {
    // X does not need to be clamped as getBlendPair handles it fine

    if (!mNodes.size()) [[unlikely]] {
        return AnimSampleBlendDataPair();
    }

    // Update X
    if (mMaxAlphaChangeSpeed == 0.0f) {
        mX = x;
    }
    else {
        // Use max speed
        f32 deltaX = x - mX;
        if (deltaX > 0.0f) {
            mX = glm::min(mX + mMaxAlphaChangeSpeed * elapsedSec, x);
        }
        else if (deltaX < 0.0f) {
            mX = glm::max(mX - mMaxAlphaChangeSpeed * elapsedSec, x);
        }
    }

    AnimSampleBlendDataPair rv;
    f32 animSpeed;
    AnimBlendPair blendPair = getBlendPair(mX, animSpeed);
    // Select loop duration based on weights
    rv.first.anim = &blendPair.anim0.getLoadedOrUnloadedAsset();
    rv.first.weight = blendPair.weight0;
    const f32 duration0 = rv.first.anim->animation.duration();
    f32 duration1;
    float loopDuration = duration0 * rv.first.weight;
    if (blendPair.hasBoth()) {
        rv.second.anim = &blendPair.anim1.getLoadedOrUnloadedAsset();
        rv.second.weight = 1.0f - blendPair.weight0;
        duration1 = rv.second.anim->animation.duration();
        loopDuration += duration1 * rv.second.weight;
        rv.validCount = 2;
    }
    else {
        duration1 = 0.0f;
        rv.validCount = 1;
    }
    assert(loopDuration > 0.0f);

    f32 loopTime = loopDuration * mSyncAlpha;
    loopTime += elapsedSec * animSpeed;
    if (loopTime > loopDuration) [[unlikely]] {
        loopTime = fmod(loopTime, loopDuration);
    }
    mSyncAlpha = loopTime / loopDuration;
    assert(mSyncAlpha >= 0.0f && mSyncAlpha <= 1.0f);

    rv.first.animTime = mSyncAlpha * duration0;
    rv.second.animTime = mSyncAlpha * duration1;
    return rv;
}

AnimBlendPair Blendspace1DPlayer::getBlendPair(f32 x, OUT f32& outAnimSpeed) const {
    assert(mNodes.size() > 0);

    // Before first node
    if (x <= mNodes[0].x) {
        const f32 dist = mNodes[0].x - x;
        const f32 ratio = dist / mNodes[0].x;
        outAnimSpeed = 1.0f - ratio * mSpeedWarpLess;
        // Sometimes can be negative here
        outAnimSpeed = glm::max(outAnimSpeed, 0.0f);
        return { mNodes[0].animId, INVALID_ASSET_ID, 1.0f };
    }

    for (size_t i = 0; i < mNodes.size() - 1; ++i) {
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
            outAnimSpeed = 1.0f;
            return rv;
        }
    }
    // After last node
    const Blendspace1DPlayerNode& lastNode = mNodes[mNodes.size() - 1];
    const f32 dist = x - lastNode.x;
    const f32 ratio = dist / lastNode.x;
    outAnimSpeed = 1.0f - ratio * mSpeedWarpGreater;
    assert(outAnimSpeed >= 0.0f);
    return { lastNode.animId, INVALID_ASSET_ID, 1.0f };
}

// TODO: This eliminates the bitArray so can be more efficient than AssetHandleBundle, use this there?
//bool Blendspace1DPlayer::areAllAssetsLoaded() const
//{
//    if (mLoadedCount == mNumNodes) [[likely]] { return true; }
//    mLoadedCount = 0;
//    for (ui32 i = 0; i < mNumNodes; ++i) {
//        if (mAnimAssetHandles[i]->isLoaded()) {
//            ++mLoadedCount;
//        }
//        else {
//            // We don't need to check every one if one fails
//            return false;
//        }
//    };
//    assert(mLoadedCount == mNumNodes);
//    return true;
//}
