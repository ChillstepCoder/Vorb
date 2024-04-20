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

typedef ozz::vector<ozz::math::SoaTransform> OzzSoaTransformVector;
typedef ozz::vector<ozz::math::Float4x4> OzzMatrixVector;

typedef ozz::span<const ozz::math::SoaTransform> OzzConstSoaTransformSpan;
typedef ozz::span<ozz::math::SoaTransform> OzzSoaTransformSpan;
typedef ozz::span<ozz::math::Float4x4> OzzMatrixSpan;

struct SkeletalAnimationSampleContext {
    ozz::animation::SamplingJob::Context samplingContext;
    const ozz::animation::Animation* anim = nullptr;
    f32 time = 0.0f;
};
