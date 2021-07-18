#pragma once

enum class IntersectionHitShape {
    NO_HIT,
    CIRCLE,
    AABB
};

// https://noonat.github.io/intersect/#aabb-vs-segment
struct IntersectionHit {
    f32v2 position;
    f32v2 delta; // overlap distances
    f32v2 normal;
    f32 time = 0.0f; // Defined only for segment and sweep
    IntersectionHitShape shape = IntersectionHitShape::NO_HIT;

    bool didHit() { return shape != IntersectionHitShape::NO_HIT; }
};