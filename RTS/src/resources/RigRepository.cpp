#include "stdafx.h"
#include "RigRepository.h"

#include <Vorb/io/IOManager.h>
#include <ozz/base/io/archive.h>
#include <ozz/base/io/stream.h>
#include <ozz/animation/runtime/skeleton.h>

RigRepository::RigRepository(vio::IOManager& ioManager) : mIoManager(ioManager) {

}

RigRepository::~RigRepository() {

}

bool RigRepository::loadRigFile(const vio::Path& filePath) {
    RigDef& def = mRigDefs.emplace_back();
    def.mRigId = mRigDefs.size() - 1u;

    if (!mIoManager.parseFileAsKegObject((ui8*)&def, filePath, &KEG_GLOBAL_TYPE(RigDef))) {
        pError("Failed to load model file " + filePath.getString());
        return false;
    }

    if (def.mSkeletonFileName.empty()) {
        pError("Rig file missing skeleton path " + filePath.getString());
        return false;
    }

    vio::Path rootDir = filePath;
    const nString rigFileNameNoExtension = filePath.getFileNameNoExtension();
    rootDir.trimEnd();
    assert(rootDir.isDirectory());

    // Load skeleton
    {
        vio::Path skeletonPath = rootDir + nString("\\") + def.mSkeletonFileName;
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
    def.mNumAnimations = def.mAnimationFileNames.size();
    if (def.mNumAnimations) {
        def.mAnimations = std::unique_ptr<ozz::animation::Animation[]>(new ozz::animation::Animation[def.mNumAnimations]);
        for (ui32 i = 0; i < def.mNumAnimations; ++i) {
            vio::Path animPath = rootDir + nString("\\") + def.mAnimationFileNames[i];
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

            archive >> def.mAnimations[i];
        }
    }

    mRigIdLookup[rigFileNameNoExtension] = def.mRigId;
}

const RigDef& RigRepository::getRigDef(const nString& name) const {
    auto&& it = mRigIdLookup.find(name);
    assert(it != mRigIdLookup.end());
    return mRigDefs[it->second];
}
