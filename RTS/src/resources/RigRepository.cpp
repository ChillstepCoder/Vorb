#include "stdafx.h"
#include "RigRepository.h"

#include "resources/AnimationRepository.h"

#include <Vorb/io/IOManager.h>
#include <ozz/base/io/archive.h>
#include <ozz/base/io/stream.h>
#include <ozz/animation/runtime/skeleton.h>
#include <ozz/animation/runtime/skeleton_utils.h>

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


AssetLoadFunc RigRepository::getAssetLoadFunc() {
    return [&]ASSET_LOAD_LAMBDA(assetID, filePath, assetDataPtr) {
        RigDef& def = *static_cast<RigDef*>(assetDataPtr);

        ryml::Tree tree = YmlSerializer::parseFileData(readFileToString(filePath));

        RigDefFileData fileData;
        tree.crootref() >> fileData;

        if (!fileData.mSkeletonFileName.isValid()) {
            pError("Rig file missing skeleton path " + filePath.getString());
            return false;
        }

        vio::Path rootDir = filePath;
        rootDir.trimEnd();
        assert(rootDir.isDirectory());

        // Load skeleton
        {
            vio::Path skeletonPath = rootDir + nString("\\") + fileData.mSkeletonFileName.toString();
            ozz::io::File file(skeletonPath.getCString(), "rb");

            if (!file.opened()) {
                panic("Skeleton import failure - {}. Ensure that it matches a valid StrToken", skeletonPath.getString());
            }

            ozz::io::IArchive archive(&file);
            if (!archive.TestTag<ozz::animation::Skeleton>()) {
                panic("Skeleton file is not a skeleton - {}", skeletonPath.getString());
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
                panic("Rig upper_root {} is not found in the skeleton {}. Ensure it is a valid strtoken ", fileData.mUpperRootJointName.c_str(), filePath.getString());
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

        AnimationRepository& animRepo = AnimationRepository::get();

        // Map joint names
        def.mJointNameToIndex.reserve(def.mSkeleton.num_joints());
        for (int i = 0; i < def.mSkeleton.num_joints(); ++i) {
            def.mJointNameToIndex[StrToken(def.mSkeleton.joint_names()[i])] = i;
        }

        //// TODO: This fileData copy is expensive, use userdata
        //assetLoader.requestAssetLoadWithDependencies([&, fileData]ASSET_LOAD_LAMBDA(AssetID, filePath, assetDataPtr) {
        //    def.mNumAnimations = fileData.mAnimationNames.size();
        //    if (def.mNumAnimations) {
        //        def.mAnimations = std::unique_ptr<ConstOzzAnimationPtr[]>(new ConstOzzAnimationPtr[def.mNumAnimations]);
        //        for (ui32 i = 0; i < def.mNumAnimations; ++i) {
        //            def.mAnimations[i] = &def.getDependencies()->getLoadedAsset<AnimationDef>(fileData.mAnimationNames[i]).mAnimation;
        //            def.mNameToAnimationIndex[fileData.mAnimationNames[i]] = i;
        //        }
        //    }
        //    return true;
        //},
        //    nullptr,
        //    assetID,
        //    assetDataPtr,
        //    filePath,
        //    mLoadedAssets[assetID].get(),
        //    nullptr,
        //    def.getDependencies()
        //);
        //return false;
        return true;
    };
}
