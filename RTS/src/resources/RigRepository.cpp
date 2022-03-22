#include "stdafx.h"
#include "RigRepository.h"

#include "resources/AnimationRepository.h"

#include <Vorb/io/IOManager.h>
#include <ozz/base/io/archive.h>
#include <ozz/base/io/stream.h>
#include <ozz/animation/runtime/skeleton.h>

RigRepository::RigRepository(vio::IOManager& ioManager) : mIoManager(ioManager) {

}

RigRepository::~RigRepository() {

}

bool RigRepository::loadRigFile(const vio::Path& filePath, const AnimationRepository& animRepo) {
    RigDef& def = mRigDefs.emplace_back();
    def.mRigId = mRigDefs.size() - 1u;

    RigDefFileData fileData;
    if (!mIoManager.parseFileAsKegObject((ui8*)&fileData, filePath, &KEG_GLOBAL_TYPE(RigDefFileData))) {
        pError("Failed to load rig file " + filePath.getString());
        return false;
    }

    if (fileData.mSkeletonFileName.empty()) {
        pError("Rig file missing skeleton path " + filePath.getString());
        return false;
    }

    vio::Path rootDir = filePath;
    rootDir.trimEnd();
    assert(rootDir.isDirectory());

    // Load skeleton
    {
        vio::Path skeletonPath = rootDir + nString("\\") + fileData.mSkeletonFileName;
        ozz::io::File file(skeletonPath.getCString(), "rb");

        if (!file.opened()) {
            pError("Skeleton import failure - " + skeletonPath.getString());
            assert(false);
        }

        ozz::io::IArchive archive(&file);
        if (!archive.TestTag<ozz::animation::Skeleton>()) {
            pError("Skeleton file is not a skeleton - " + skeletonPath.getString());
            assert(false);
        }

        archive >> def.mSkeleton;
    }

    // Load animations
    def.mNumAnimations = fileData.mAnimationNames.size();
    if (def.mNumAnimations) {
        def.mAnimations = std::unique_ptr<ConstOzzAnimationPtr[]>(new ConstOzzAnimationPtr[def.mNumAnimations]);
        for (ui32 i = 0; i < def.mNumAnimations; ++i) {
            def.mAnimations[i] = &animRepo.getAnimation(fileData.mAnimationNames[i]);
            def.mNameToAnimationIndex[fileData.mAnimationNames[i]] = i;
        }
    }

    const nString rigFileNameNoExtension = filePath.getFileNameNoExtension();
    assert(mRigIdLookup.find(rigFileNameNoExtension) == mRigIdLookup.end());
    mRigIdLookup[rigFileNameNoExtension] = def.mRigId;
    return true;
}

const RigDef& RigRepository::getRigDef(const nString& name) const {
    auto&& it = mRigIdLookup.find(name);
    assert(it != mRigIdLookup.end());
    return mRigDefs[it->second];
}

const RigDef* RigRepository::tryGetRigDef(const nString& name) const {
    auto&& it = mRigIdLookup.find(name);
    if (it == mRigIdLookup.end()) {
        return nullptr;
    }
    return &mRigDefs[it->second];
}
