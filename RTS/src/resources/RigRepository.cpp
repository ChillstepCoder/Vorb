#include "stdafx.h"
#include "RigRepository.h"

#include "resources/AnimationRepository.h"

#include <Vorb/io/IOManager.h>
#include <ozz/base/io/archive.h>
#include <ozz/base/io/stream.h>
#include <ozz/animation/runtime/skeleton.h>
#include <ozz/animation/runtime/skeleton_utils.h>

RigRepository::RigRepository(vio::IOManager& ioManager) : mIoManager(ioManager) {

}

RigRepository::~RigRepository() {

}

// Helper functor used to set weights while traversing joints hierarchy.
struct WeightSetupIterator {
    WeightSetupIterator(ozz::vector<ozz::math::SimdFloat4>* _weights,
        float _weight_setting)
        : weights(_weights), weight_setting(_weight_setting) {}
    void operator()(int _joint, int) {
        ozz::math::SimdFloat4& soa_weight = weights->at(_joint / 4);
        soa_weight = ozz::math::SetI(
            soa_weight, ozz::math::simd_float4::Load1(weight_setting),
            _joint % 4);
    }
    ozz::vector<ozz::math::SimdFloat4>* weights;
    float weight_setting;
};

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

    // Set upper body weight mask
    def.mUpperBodyJointWeights.resize(def.mSkeleton.num_soa_joints());
    def.mLowerBodyJointWeights.resize(def.mSkeleton.num_soa_joints());
    if (fileData.mUpperRootJointName.size()) {
        // Zero out all joints
        for (int i = 0; i < def.mSkeleton.num_soa_joints(); ++i) {
            def.mUpperBodyJointWeights[i] = ozz::math::simd_float4::zero();
            def.mLowerBodyJointWeights[i] = ozz::math::simd_float4::one();
        }
        // Find the upper root joint
        const int upperBodyRootJointIndex = ozz::animation::FindJoint(def.mSkeleton, fileData.mUpperRootJointName.c_str());
        if (upperBodyRootJointIndex < 0) {
            pError("Rig upper_root is not found in the skeleton " + filePath.getString());
        }
        // DFS iterate joints from the upper body root and set to 1.0f for upper
        WeightSetupIterator upper_it(&def.mUpperBodyJointWeights, 1.0f);
        ozz::animation::IterateJointsDF(def.mSkeleton, upper_it, upperBodyRootJointIndex);
        
        // DFS iterate joints from the upper body root and set to 0.0f for lower
        WeightSetupIterator lower_it(&def.mLowerBodyJointWeights, 0.0f);
        ozz::animation::IterateJointsDF(def.mSkeleton, lower_it, upperBodyRootJointIndex);

    }
    else {
        // We have no upper body so just set it all to one
        for (int i = 0; i < def.mSkeleton.num_soa_joints(); ++i) {
            def.mUpperBodyJointWeights[i] = ozz::math::simd_float4::one();
            def.mLowerBodyJointWeights[i] = ozz::math::simd_float4::one();
        }
    }

    // Hook up animations
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
