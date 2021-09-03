#pragma once

enum class IntersectionHitShape {
    NO_HIT,
    CIRCLE,
    AABB
};

// https://noonat.github.io/intersect/#aabb-vs-segment
struct IntersectionHit2D {
    f32v2 position;
    f32v2 delta; // overlap distances
    f32v2 normal;
    f32 time = 0.0f; // Defined only for segment and sweep
    IntersectionHitShape shape = IntersectionHitShape::NO_HIT;

    bool didHit() { return shape != IntersectionHitShape::NO_HIT; }
};

struct IntersectionHit3D {
    f32v3 position;
    f32 closeTime = 0.0f;
    f32 farTime = 0.0f;
    IntersectionHitShape shape = IntersectionHitShape::NO_HIT;

    bool didHit() { return shape != IntersectionHitShape::NO_HIT; }
};