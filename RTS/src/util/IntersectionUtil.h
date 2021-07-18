#pragma once

#include "IntersectionHit.h"

namespace IntersectionUtil
{
    bool segmentSegmentIntersect(const f32v2& a1, const f32v2& a2, const f32v2& b1, const f32v2& b2, OUT f32v2* intersection);
    IntersectionHit segmentAABBIntersect(const f32v2& pos, const f32v2& offset, const f32v2& aabbCenter, const f32v2& aabbRadii, f32v2 padding = f32v2(0.0f));
    IntersectionHit segmentCircleIntersect(const f32v2& p1, const f32v2& p2, const f32v2& circleCenter, const f32 radius, f32 padding = 0.0f);
    bool segmentAABBIntersectBoolean(const f32v2& aabbMin, const f32v2& aabbMax, const f32v2& p1, const f32v2& p2);
};

