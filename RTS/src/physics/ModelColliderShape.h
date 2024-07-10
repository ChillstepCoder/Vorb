#pragma once

struct ModelColliderShape {
    f32v3 mEulerAngles = f32v3(M_PI_2F, 0.0f, 0.0f);
    f32v3 mOffset = f32v3(0.0f);
    CollisionShapes mShape = CollisionShapes::Capsule;
    f32v3 mHalfDims = f32v3(0.5f);
};
SERIALIZABLE_SIMPLE(ModelColliderShape,
    make_field(o.mEulerAngles, "angles"sv),
    make_field(o.mOffset, "offset"sv),
    make_field(o.mShape, "shape"sv),
    make_field(o.mHalfDims, "dims"sv)
);