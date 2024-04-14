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
    // returns false on sampling error, stores result affine transforms in outTransforms, must be sent through skinMatricesToMesh to be used
    static bool samplePose(SkeletalAnimationSampleContext& context, const RigDef& rig, OzzSoaTransformVector& outTransforms);
    // TODO: Blend
    // Take finished local Soa transforms and convert them to model matrices
    static bool localToModel(const OzzSoaTransformVector& transforms, const RigDef& rig, OzzMatrixVector& outModelMatrices);
    // Map matrices to a specific mesh for skinning
    static bool skinModelMatricesToMesh(OzzMatrixVector& outSkinningMatrices, const OzzMatrixVector& modelMatrices, const MeshSkeletonData& meshData);
};

