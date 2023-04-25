#pragma once

struct IntersectionHit2D;
struct IntersectionHit3D;

namespace IntersectionUtil
{
    bool segmentSegmentIntersect(const f32v2& a1, const f32v2& a2, const f32v2& b1, const f32v2& b2, OUT f32v2* intersection);
    IntersectionHit2D segmentAABBIntersect(const f32v2& pos, const f32v2& offset, const f32v2& aabbCenter, const f32v2& aabbRadii, f32v2 padding = f32v2(0.0f));
    IntersectionHit2D segmentCircleIntersect(const f32v2& p1, const f32v2& p2, const f32v2& circleCenter, const f32 radius, f32 padding = 0.0f);
    // r1 and r2 should be prenormalized
    IntersectionHit2D rayRayIntersect(const f32v2& p1, const f32v2& r1, const f32v2& p2, const f32v2& r2);
    bool segmentAABBIntersectBoolean(const f32v2& aabbMin, const f32v2& aabbMax, const f32v2& p1, const f32v2& p2);
    IntersectionHit3D LineAABBIntersection(const f32AABB3& aabbBox, const f32v3& v0, const f32v3& v1);
    IntersectionHit3D RaySphereIntersection(const BoundingSphere& sphere, const f32v3& v0, const f32v3& v1);
    IntersectionHit3D RayTriangleIntersection(const f32v3& v0, const f32v3& v1, const f32v3& t0, const f32v3& t1, const f32v3& t2);
};

