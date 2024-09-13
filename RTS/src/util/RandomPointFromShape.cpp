#include "stdafx.h"
#include "RandomPointFromShape.h"

f32v3 util::queryRandomPointFromSphere(f32 radius, ui32 seed /*= UINT32_MAX*/) {
    if (seed == UINT32_MAX) seed = Random::getCachedRandom();
    f32 theta = 2.f * M_PIF * Random::getCachedRandomfSpecific(seed); // azimuthal angle
    f32 phi = acosf(2.f * Random::getCachedRandomfSpecific(seed + 100) - 1.f); // polar angle
    f32 r = radius * Random::getCachedRandomfSpecific(seed + 200); // cube root to ensure points are uniformly distributed

    f32v3 point;
    point.x = r * sin(phi) * cos(theta);
    point.y = r * sin(phi) * sin(theta);
    point.z = r * cos(phi);
    return point;
}

f32v3 util::queryRandomPointFromBox(f32v3 halfExtents, ui32 seed /*= UINT32_MAX*/) {
    if (seed == UINT32_MAX) seed = Random::getCachedRandom();
    f32 xAlpha = Random::getCachedRandomfSpecific(seed) * 2.0f - 1.0f;
    f32 yAlpha = Random::getCachedRandomfSpecific(seed + 1) * 2.0f - 1.0f;
    f32 zAlpha = Random::getCachedRandomfSpecific(seed + 2) * 2.0f - 1.0f;

    f32v3 point;
    point.x = halfExtents.x * xAlpha;
    point.y = halfExtents.y * yAlpha;
    point.z = halfExtents.z * zAlpha;
    return point;
}
