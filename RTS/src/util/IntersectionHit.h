#pragma once

// TODO: REMOVE

enum class IntersectionHitShape {
    NO_HIT,
    CIRCLE,
    AABB,
    TRIANGLE,
    SPHERE,
    RAY
};

// https://noonat.github.io/intersect/#aabb-vs-segment
struct IntersectionHit2D {
    f32v2 position;
    f32v2 delta; // overlap distances
    f32v2 normal;
    ui32v2 tilePos;
    f32 time = 0.0f; // Defined only for segment and sweep
    IntersectionHitShape shape = IntersectionHitShape::NO_HIT;

    bool didHit() { return shape != IntersectionHitShape::NO_HIT; }
};

struct IntersectionHit3D {
    f32v3 position;
    f32 closeTime = FLT_MAX;
    f32 farTime = FLT_MAX;
    IntersectionHitShape shape = IntersectionHitShape::NO_HIT;

    bool didHit() const { return shape != IntersectionHitShape::NO_HIT; }
};