#pragma once

// TODO: Move?
#include <ozz/base/containers/vector.h>
#include <ozz/base/maths/simd_math.h>
#include <ozz/base/maths/soa_transform.h>
#include <ozz/animation/runtime/sampling_job.h>
#include <ozz/base/span.h>
#include <ozz/animation/runtime/local_to_model_job.h>
#include <ozz/animation/runtime/sampling_job.h>
#include <ozz/animation/runtime/blending_job.h>

class RigDef;
struct MeshSkeletonData;

struct SkeletalAnimationContext {
    std::unique_ptr<ozz::animation::SamplingJob::Context> mSamplingContext; // TODO: Pool allocator
    const ozz::animation::Animation* anim;
    f32 time = 0.0f;
};

class SkeletalAnimator {
public:
    ozz::vector<ozz::math::Float4x4>* getPoseAtTime(const SkeletalAnimationContext& context, const RigDef& rig, const MeshSkeletonData& skeletonData);

private:
    ozz::vector<ozz::math::Float4x4> models;
    ozz::vector<ozz::math::Float4x4> skinningMatrices;
    ozz::vector<ozz::math::SoaTransform> locals[NUM_ANIM_STATE_TRACKS + 1];
    ozz::vector<ozz::math::SoaTransform> blendedLocals;
};

