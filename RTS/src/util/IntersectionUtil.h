#pragma once

struct IntersectionHit2D;
struct IntersectionHit3D;

namespace IntersectionUtil
{
    bool segmentSegmentIntersect(const f32v2& a1, const f32v2& a2, const f32v2& b1, const f32v2& b2, OUT f32v2* intersection);
    IntersectionHit2D segmentAABBIntersect(const f32v2& pos, const f32v2& offset, const f32v2& aabbCenter, const f32v2& aabbRadii, f32v2 padding = f32v2(0.0f));
    IntersectionHit2D segmentCircleIntersect(const f32v2& p1, const f32v2& p2, const f32v2& circleCenter, const f32 radius, f32 padding = 0.0f);
    // r1 and r2
    IntersectionHit2D rayRayIntersect(const f32v2& p1, const f32v2& r1, const f32v2& p2, const f32v2& r2);
    // Test s against t
    IntersectionHit2D segmentSegmentIntersect(f32v2 s1, f32v2 s2, f32v2 t1, f32v2 t2);
    // Test s against t,r
    IntersectionHit2D segmentRayIntersect(f32v2 s1, f32v2 s2, f32v2 t, f32v2 r);
    // Test s,r against t
    IntersectionHit2D raySegmentIntersect(f32v2 s, f32v2 r, f32v2 t1, f32v2 t2);
    // Test s,r against t,n
    IntersectionHit2D rayLineIntersect(f32v2 s, f32v2 r, f32v2 t, f32v2 n);
    // Test t,n against s,r
    IntersectionHit2D lineRayIntersect(f32v2 s, f32v2 n, f32v2 t, f32v2 r);
    // Test s against t,n
    IntersectionHit2D segmentLineIntersect(f32v2 s1, f32v2 s2, f32v2 t, f32v2 n);
    // Test t against s,r
    IntersectionHit2D lineSegmentIntersect(f32v2 s, f32v2 n, f32v2 t1, f32v2 t2);
    // Test t,n1 against t,n2
    IntersectionHit2D lineLineIntersect(f32v2 s, f32v2 n1, f32v2 t, f32v2 n2);

    bool segmentAABBIntersectBoolean(const f32v2& aabbMin, const f32v2& aabbMax, const f32v2& p1, const f32v2& p2);
    IntersectionHit3D LineAABBIntersection(const f32AABB3& aabbBox, const f32v3& v0, const f32v3& v1);
    IntersectionHit3D RaySphereIntersection(const BoundingSphere& sphere, const f32v3& v0, const f32v3& v1);
    IntersectionHit3D RayTriangleIntersection(const f32v3& v0, const f32v3& v1, const f32v3& t0, const f32v3& t1, const f32v3& t2);
};

