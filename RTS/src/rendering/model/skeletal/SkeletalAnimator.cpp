#include "stdafx.h"
#include "SkeletalAnimator.h"

#include "rendering/mesh/MeshSkeletonData.h"

#include "definitions/RigDef.h"

ozz::vector<ozz::math::Float4x4>* SkeletalAnimator::getPoseAtTime(const SkeletalAnimationContext& context, const RigDef& rig, const MeshSkeletonData& skeletonData) {
    f32 blendWeights[NUM_ANIM_STATE_TRACKS + 1];

    // Animation and skinning
    const int numSoaJoints = rig.mSkeleton.num_soa_joints();
    const int numJoints = rig.mSkeleton.num_joints();
    models.resize(numJoints);

    // Allocate buffers
    locals[0].resize(numSoaJoints);
    // Sample animation
    ozz::animation::SamplingJob sampling_job;
    sampling_job.animation = anim;
    sampling_job.context = currentTrack.mContext.get();
    sampling_job.ratio = currentTrack.mTime / currentTrack.mDuration;
    sampling_job.output = make_span(locals[numValidTracks]);
    blendWeights[numValidTracks] = currentTrack.getTotalWeight();
    ++numValidTracks;
    if (!sampling_job.Run()) {
        pError("Sampling job error");
        return nullptr;
    }

    // One shot animation
    f32 oneShotWeight = 0.0f;
    AnimTrack& oneShotTrack = animState.mCurrentOneShotTrack;
    if (oneShotTrack.isActive()) {
        oneShotWeight = oneShotTrack.getTotalWeight();
        // Allocate buffers
        locals[numValidTracks].resize(numSoaJoints);
        // Sample animation
        ozz::animation::SamplingJob sampling_job;
        sampling_job.animation = animState.mCurrentOneShotAnimation;
        sampling_job.context = oneShotTrack.mContext.get();
        sampling_job.ratio = oneShotTrack.mTime / oneShotTrack.mDuration;
        sampling_job.output = make_span(locals[numValidTracks]);
        blendWeights[numValidTracks] = oneShotWeight;
        if (!sampling_job.Run()) {
            pError("Sampling job error");
            return nullptr;
        }

        // Increment timers
        oneShotTrack.update(elapsedSec, animState.mFootstepAlpha);
    }

    // Converts from local space to model space matrices.
    ozz::animation::LocalToModelJob ltm_job;
    ltm_job.skeleton = &rig.mSkeleton;

    // Blending
    if (numValidTracks > 1 || oneShotWeight) {

        const f32 inverseOneShotWeightMult = 1.0f - oneShotWeight;
        int totalLayers = 0;
        // Prepares blending layers.
        ozz::animation::BlendingJob::Layer layers[(NUM_ANIM_STATE_TRACKS + 1) * 2]; // Account for splitting layers

        // While one shots are active, blending is more complex as we must split lower and upper body blending
        if (oneShotWeight) {
            // Split all layers into upper and lower body portions based on the one shot weight
            if (numValidTracks > 0) {
                if (oneShotWeight == 1.0f) {
                    // If we are at full weight, we have no upper body layers
                    for (ui32 i = 0; i < numValidTracks; ++i) {
                        layers[totalLayers].transform = make_span(locals[i]);
                        layers[totalLayers].weight = blendWeights[i];
                        layers[totalLayers].joint_weights = make_span(rig.mLowerBodyJointWeights);
                        ++totalLayers;
                    }
                }
                else {
                    // Split into two layers for upper and lower portion
                    const f32 upperBodyWeight = 1.0f - oneShotWeight;
                    for (ui32 i = 0; i < numValidTracks; ++i) {
                        // Lower body
                        layers[totalLayers].transform = make_span(locals[i]);
                        layers[totalLayers].weight = blendWeights[i];
                        layers[totalLayers].joint_weights = make_span(rig.mLowerBodyJointWeights);
                        ++totalLayers;

                        // Upper body
                        layers[totalLayers].transform = make_span(locals[i]);
                        layers[totalLayers].weight = blendWeights[i] * upperBodyWeight;
                        layers[totalLayers].joint_weights = make_span(rig.mUpperBodyJointWeights);
                        ++totalLayers;
                    }
                }

                // The final layer is the one shot layer, flag it upper body only if we are in motion
                // TODO: Need to fade this in as well to prevent pop?
                if (locomotionMode != CharacterLocomotionMode::IDLE) {
                    layers[totalLayers].joint_weights = make_span(rig.mUpperBodyJointWeights);
                }
            }

            // Add one shot locals
            layers[totalLayers].transform = make_span(locals[numValidTracks]);
            layers[totalLayers].weight = blendWeights[numValidTracks];
            ++totalLayers;
        }
        else {
            // No one shot, standard, cheap full blending for each anim
            for (ui32 i = 0; i < numValidTracks; ++i) {
                layers[totalLayers].transform = make_span(locals[i]);
                layers[totalLayers].weight = blendWeights[i];
                ++totalLayers;
            }
        }

        // Setups blending job.
        ozz::animation::BlendingJob blend_job;
        blend_job.threshold = 0.015f;
        blend_job.layers = ozz::span{ layers, size_t(totalLayers) };
        blend_job.rest_pose = rig.mSkeleton.joint_rest_poses();
        blend_job.output = make_span(blendedLocals);

        // Blends.
        if (!blend_job.Run()) {
            pError("Blending job error");
            return nullptr;
        }
        ltm_job.input = make_span(blendedLocals);
    }
    else {
        ltm_job.input = make_span(locals[0]);
    }

    // Run the final job
    if (numValidTracks) {
        ltm_job.output = make_span(models);
        if (!ltm_job.Run()) {
            pError("Local to model job error");
            return nullptr;
        }

        // Compute skinning matrices
        const ozz::math::Float4x4* bindPoses = skeletonData.mInverseBindPoses.get();

        skinningMatrices.resize(skeletonData.mNumJoints);
        for (size_t i = 0; i < skeletonData.mNumJoints; ++i) {
            skinningMatrices[i] = models[skeletonData.mJointRemaps[i]] * bindPoses[i];
        }

        return &skinningMatrices;
    }
    return nullptr;
}
