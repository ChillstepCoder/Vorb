#pragma once
#include <ozz/animation/runtime/animation.h>

class AnimationRepository
{
public:
    AnimationRepository();
    ~AnimationRepository();

    bool loadAnimFile(const vio::Path& filePath);

    const ozz::animation::Animation& getAnimation(ui32 animId) const { return mAnimations[animId]; }
    const ozz::animation::Animation& getAnimation(const nString& name) const;
    const ozz::animation::Animation* tryGetAnimation(const nString& name) const;

private:
    std::unordered_map<nString, ui32> mAnimIdLookups;
    std::vector<ozz::animation::Animation> mAnimations;

};