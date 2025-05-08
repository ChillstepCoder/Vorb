#pragma once

#include "rendering/model/skeletal/SkeletalAnimationSampleContext.h"

class RigDef;
struct MeshSkeletonData;

// Animation steps:
// 1. Sample pose
// 2. Blend
// 3. Local to Model
// 4. Per submesh - skinToMesh
class SkeletalAnimator {
public:
    // returns false on sampling error, stores result affine transforms in outLocals, must be sent through skinMatricesToMesh to be used
    static bool samplePose(SkeletalAnimationSampleContext& context, const RigDef& rig, OzzSoaTransformSpan outLocals);
    // TODO: Blend
    static bool blendPoses(ozz::span<const ozz::animation::BlendingJob::Layer> layers, const RigDef& rig, OzzSoaTransformSpan outLocals);
    // Take finished local Soa transforms and convert them to model matrices
    static bool localToModel(const OzzConstSoaTransformSpan transforms, const RigDef& rig, OzzMatrixSpan outModelMatrices);
    // Map matrices to a specific mesh for skinning
    static bool skinModelMatricesToMesh(const OzzMatrixSpan& modelMatrices, const MeshSkeletonData& meshData, OzzMatrixSpan outSkinningMatrices);

};

