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

typedef ozz::vector<ozz::math::SoaTransform> OzzSoaTransformVector;
typedef ozz::vector<ozz::math::Float4x4> OzzMatrixVector;

struct SkeletalAnimationContext {
    ozz::animation::SamplingJob::Context samplingContext;
    const ozz::animation::Animation* anim;
    f32 time = 0.0f;
};
// Animation steps:
// 1. Sample pose
// 2. Blend
// 3. Local to Model
// 4. Per submesh - skinToMesh
class SkeletalAnimator {
public:
    // returns false on sampling error, stores result affine transforms in outTransforms, must be sent through skinMatricesToMesh to be used
    static bool samplePose(SkeletalAnimationContext& context, const RigDef& rig, OzzSoaTransformVector& outTransforms);
    // TODO: Blend
    // Take finished local Soa transforms and convert them to model matrices
    static bool localToModel(const OzzSoaTransformVector& transforms, const RigDef& rig, OzzMatrixVector& outModelMatrices);
    // Map matrices to a specific mesh for skinning
    static bool skinModelMatricesToMesh(OzzMatrixVector& outSkinningMatrices, const OzzMatrixVector& modelMatrices, const MeshSkeletonData& meshData);
};

