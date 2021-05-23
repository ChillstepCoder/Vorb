#include "stdafx.h"

#include "IntersectionUtil.h"

bool IntersectionUtil::segmentSegmentIntersect(const f32v2& a1, const f32v2& a2, const f32v2& b1, const f32v2& b2, OUT f32v2* intersection)
{
    f32v2 b = a2 - a1;
    f32v2 d = b2 - b1;
    float bDotDPerp = b.x * d.y - b.y * d.x;

    // if b dot d == 0, it means the lines are parallel so have infinite intersection points
    if (bDotDPerp == 0)
        return false;

    f32v2 c = b1 - a1;
    float t = (c.x * d.y - c.y * d.x) / bDotDPerp;
    if (t < 0 || t > 1)
        return false;

    float u = (c.x * b.y - c.y * b.x) / bDotDPerp;
    if (u < 0 || u > 1)
        return false;

    if (intersection) {
        *intersection = a1 + t * b;
    }

    return true;
}
//https://stackoverflow.com/questions/99353/how-to-test-if-a-line-segment-intersects-an-axis-aligned-rectange-in-2d
bool IntersectionUtil::segmentAABBIntersectBoolean(const f32v2& aabbMin, const f32v2& aabbMax, const f32v2& p1, const f32v2& p2) {
    // Find min and max X for the segment

    f32 minX = p1.x;
    f32 maxX = p2.x;

    if (p1.x > p2.x) {
        minX = p2.x;
        maxX = p1.x;
    }

    // Find the intersection of the segment's and rectangle's x-projections
    if (maxX > aabbMax.x) {
        maxX = aabbMax.x;
    }

    if (minX < aabbMin.x) {
        minX = aabbMin.x;
    }

    if (minX > maxX) { // If x projections do not intersect return false
        return false;
    }

    // Find corresponding min and max Y for min and max X we found before

    f32 minY = p1.y;
    f32 maxY = p2.y;

    f32 dx = p2.x - p1.x;

    if (abs(dx) > 0.0000001f) {
        f32 a = (p2.y - p1.y) / dx;
        f32 b = p1.y - a * p1.x;
        minY = a * minX + b;
        maxY = a * maxX + b;
    }

    if (minY > maxY) {
        f32 tmp = maxY;
        maxY = minY;
        minY = tmp;
    }

    // Find the intersection of the segment's and rectangle's y-projections
    if (maxY > aabbMax.y) {
        maxY = aabbMax.y;
    }

    if (minY < aabbMin.y) {
        minY = aabbMin.y;
    }

    if (minY > maxY) { // If Y-projections do not intersect return false
        return false;
    }

    return true;
}

template <typename T> int sgn(T val) {
    return (T(0) < val) - (val < T(0));
}

//https://noonat.github.io/intersect/#aabb-vs-segment
IntersectionHit IntersectionUtil::segmentAABBIntersect(const f32v2& pos, const f32v2& offset, const f32v2& aabbCenter, const f32v2& aabbRadii, f32v2 padding /*= f32v2(0.0f)*/) {
    const f32 scaleX = 1.0f / offset.x;
    const f32 scaleY = 1.0f / offset.y;
    const f32 signX = sgn(scaleX);
    const f32 signY = sgn(scaleY);
    const f32 nearTimeX = (aabbCenter.x - signX * (aabbRadii.x + padding.x) - pos.x) * scaleX;
    const f32 nearTimeY = (aabbCenter.y - signY * (aabbRadii.y + padding.y) - pos.y) * scaleY;
    const f32 farTimeX = (aabbCenter.x + signX * (aabbRadii.x + padding.x) - pos.x) * scaleX;
    const f32 farTimeY = (aabbCenter.y + signY * (aabbRadii.y + padding.y) - pos.y) * scaleY;
    if (nearTimeX > farTimeY || nearTimeY > farTimeX) {
        return IntersectionHit();
    }
    const f32 nearTime = nearTimeX > nearTimeY ? nearTimeX : nearTimeY;
    const f32 farTime = farTimeX < farTimeY ? farTimeX : farTimeY;

    if (nearTime >= 1 || farTime <= 0) {
        return IntersectionHit();
    }

    IntersectionHit hit;
    hit.time = vmath::clamp(nearTime, 0.0f, 1.0f);
    if (nearTimeX > nearTimeY) {
        hit.normal.x = -signX;
        hit.normal.y = 0;
    }
    else {
        hit.normal.x = 0;
        hit.normal.y = -signY;
    }

    hit.delta.x = (1.0f - hit.time) * -offset.x;
    hit.delta.y = (1.0f - hit.time) * -offset.y;
    hit.position.x = pos.x + offset.x * hit.time;
    hit.position.y = pos.y + offset.y * hit.time;
    hit.shape = IntersectionHitShape::AABB;
    return hit;
}

// https://gamedev.stackexchange.com/questions/18422/line-segment-circle-intersection-x-value-seems-wrong
IntersectionHit IntersectionUtil::segmentCircleIntersect(const f32v2& p1, const f32v2& p2, const f32v2& circleCenter, const f32 radius, f32 padding /*= 0.0f*/)
{
    IntersectionHit hit;

    const f32 adjustedRadius = radius + padding;

    // First up, let's normalise our vectors so the circle is on the origin
    const f32v2 normA = p1 - circleCenter;
    f32 normADistSq = glm::dot(normA, normA);
    if (normADistSq < SQ(adjustedRadius)) {
        // Trivial, point starts inside radius. Collide outwards
        hit.position = circleCenter + (normA / sqrt(normADistSq)) * adjustedRadius;
        hit.delta = hit.position - circleCenter;
        hit.normal = glm::normalize(hit.delta);
        hit.shape = IntersectionHitShape::CIRCLE;
        return hit;
    }

    const f32v2 normB = p2 - circleCenter;

    const f32v2 d = normB - normA;

    // Want to solve as a quadratic equation, need 'a','b','c' components
    float aa = glm::dot(d, d);
    float bb = 2 * (glm::dot(normA, d));
    float cc = normADistSq - SQ(adjustedRadius);

    // Get determinant to see if LINE intersects
    double deter = SQ(bb) - 4 * aa * cc;
    if (deter > 0)
    {
        f32 sqrtDeter = sqrt(deter);
        f32 q; // Holds the solution to the quadratic equation
        if (bb >= 0) {
            q = (-bb - sqrtDeter) / 2.0f;
        }
        else {
            q = (-bb + sqrtDeter) / 2.0f;
        }

        f32 t1 = q / aa;
        f32 t2 = cc / q;

        // Figure out which point is closer
        if (t2 < t1) {
            std::swap(t1, t2);
        }

        if (0.0 <= t1 && t1 <= 1.0) {
            // Interpolate to get collision point
            hit.position = circleCenter + vmath::lerp(normA, normB, t1);
        }
        else if (0.0 <= t2 && t2 <= 1.0) {
            hit.position = circleCenter + vmath::lerp(normA, normB, t2);
        }
        else {
            return hit; // No hit
        }

        hit.delta = hit.position - circleCenter;
        hit.normal = glm::normalize(hit.delta);
        hit.shape = IntersectionHitShape::CIRCLE;
        assert(hit.delta.x == hit.delta.x); // NAN CHECK
    }
    // So find the distance that places the intersection point right at 
    // the radius.  This is the center of the circle at the time of collision
    // and is different than the result from Doswa
    return hit; // No hit
}
