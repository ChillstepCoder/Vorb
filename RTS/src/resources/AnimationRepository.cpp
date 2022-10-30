#include "stdafx.h"
#include "AnimationRepository.h"

#include <fstream>
#include <ozz/base/io/archive.h>
#include <ozz/base/io/stream.h>

AnimationRepository::AnimationRepository() {

}

AnimationRepository::~AnimationRepository() {
  
}

bool AnimationRepository::loadAnimFile(const vio::Path& filePath) {
   
    ozz::io::File file(filePath.getCString(), "rb");

    if (!file.opened()) {
        pError("Animation import failure - " + filePath.getString());
        assert(false);
    }

    ozz::io::IArchive archive(&file);
    if (!archive.TestTag<ozz::animation::Animation>()) {
        pError("Animation file is not an animation - " + filePath.getString());
        assert(false);
    }
    ozz::animation::Animation& anim = mAnimations.emplace_back();
    archive >> anim;

    // Remove extension
    nString animFileNameNoExtension = filePath.getFileNameNoExtension();
    assert(mAnimIdLookups.find(animFileNameNoExtension) == mAnimIdLookups.end());
    mAnimIdLookups[animFileNameNoExtension] = mAnimations.size() - 1u;
    
    return true;
}

const ozz::animation::Animation& AnimationRepository::getAnimation(const nString& name) const {
    auto&& it = mAnimIdLookups.find(name);
    assert(it != mAnimIdLookups.end());
    return mAnimations[it->second];
}

const ozz::animation::Animation* AnimationRepository::tryGetAnimation(const nString& name) const {
    auto&& it = mAnimIdLookups.find(name);
    if (it == mAnimIdLookups.end()) {
        return nullptr;
    }
    return &mAnimations[it->second];
}

const AnimationID& AnimationRepository::getAnimationID(const nString& name) const {
    auto&& it = mAnimIdLookups.find(name);
    assert(it != mAnimIdLookups.end());
    return it->second;
}
