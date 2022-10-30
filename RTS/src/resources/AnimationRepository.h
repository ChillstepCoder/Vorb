#pragma once
#include <ozz/animation/runtime/animation.h>

#include "rendering/model/AnimationConst.h"

class AnimationRepository
{
public:
    AnimationRepository();
    ~AnimationRepository();

    bool loadAnimFile(const vio::Path& filePath);

    const Animation& getAnimation(AnimationID animId) const { return mAnimations[animId]; }
    const Animation& getAnimation(const nString& name) const;
    const Animation* tryGetAnimation(const nString& name) const;
    const AnimationID& getAnimationID(const nString& name) const;

private:
    std::unordered_map<nString, AnimationID> mAnimIdLookups;
    std::vector<ozz::animation::Animation> mAnimations;

};