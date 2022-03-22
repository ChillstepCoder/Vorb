#include "stdafx.h"
#include "AnimationRepository.h"

#include <fstream>
#include <Vorb/io/IOManager.h>
#include <ozz/base/io/archive.h>
#include <ozz/base/io/stream.h>

AnimationRepository::AnimationRepository(vio::IOManager& ioManager) : mIoManager(ioManager) {

}

AnimationRepository::~AnimationRepository() {

}

bool AnimationRepository::loadAnimFile(const vio::Path& filePath) {
    std::ifstream ifstr(filePath.getCString(), std::ifstream::in);

    constexpr int BUFFER_SIZE = 256;
    char buffer[BUFFER_SIZE];
    while (ifstr.getline(buffer, BUFFER_SIZE)) {
        vio::Path animPath = filePath.getString() + "\\" + buffer;
        ozz::io::File file(animPath.getCString(), "rb");

        if (!file.opened()) {
            pError("Animation import failure - " + animPath.getString());
            assert(false);
        }

        ozz::io::IArchive archive(&file);
        if (!archive.TestTag<ozz::animation::Animation>()) {
            pError("Animation file is not an animation - " + animPath.getString());
            assert(false);
        }
        ozz::animation::Animation& anim = mAnimations.emplace_back();
        archive >> anim;

        // Remove extension
        vio::Path fileNameTrim = buffer;
        mAnimIdLookups[fileNameTrim.getFileNameNoExtension()] = mAnimations.size() - 1u;
    }

    ifstr.close();
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
